use crate::game_files::{UpdateArchive, UpdateData, UpdateFile, UpdateFileData};
use crate::{extend::*, global::*, http, misc, println_info};
use indicatif::ProgressBar;
use log::info;
use std::fs::File;
use std::io::BufReader;
use std::path::PathBuf;
use std::{fs, path::Path, sync::Arc};
use zip::ZipArchive;

/// Retrieves the total count of files to verify (Count of all direct files + count of all files in archives).
fn get_total_verify_count(update_data: &UpdateData) -> usize {
    let archive_file_count = update_data
        .archives
        .iter()
        .map(|archive| archive.files.len())
        .sum::<usize>();

    update_data.files.len() + archive_file_count
}

/// Verifies whether a file needs to be updated
fn verify_file_needs_download(
    file_data: &UpdateFileData,
    dir: &Path,
    hashes: &mut std::collections::HashMap<String, String>,
) -> bool {
    let file_path = dir.join(&file_data.path);
    if !file_path.exists() {
        log::debug!("File {} does not exist, will download", file_data.path);
        true
    } else {
        let hash_remote = file_data.blake3.to_lowercase();
        let hash_local = hashes
            .get(&file_data.path)
            .cloned()
            .unwrap_or_else(|| file_path.get_blake3().unwrap())
            .to_lowercase();

        if hash_local != hash_remote {
            true
        } else {
            log::info!("File {} is up to date", file_data.path);
            hashes.insert(file_data.path.clone(), file_data.blake3.to_lowercase());
            false
        }
    }
}

/// Verifies all files of the update data.
/// If any direct file is outdated, it is added to the list of files.
/// If any file of an archive is outdated, it is added to the list of archives.
fn verify_files<'a>(
    update_data: &'a UpdateData,
    dir: &Path,
    hashes: &mut std::collections::HashMap<String, String>,
) -> Result<(Vec<&'a UpdateArchive>, Vec<&'a UpdateFile>), Box<dyn std::error::Error>> {
    log::info!("Checking {} files for updates", update_data.files.len());

    let pb = ProgressBar::new(get_total_verify_count(update_data) as u64);
    let pb_style = indicatif::ProgressStyle::with_template(
        "{spinner:.white} Checking files... {pos:>6} / {len:>6} done ({percent:>3}%)",
    );
    pb.set_style(pb_style.unwrap());

    let pb_arc = Arc::new(pb);
    let mut files_to_download: Vec<&UpdateFile> = Vec::new();
    let mut archives_to_download: Vec<&UpdateArchive> = Vec::new();

    for file in &update_data.files {
        if verify_file_needs_download(&file.file_data, dir, hashes) {
            files_to_download.push(file);
        }

        pb_arc.inc(1);
    }

    for archive in &update_data.archives {
        let mut verified_file_count = 0u64;
        let any_file_of_archive_needs_download = archive.files.iter().any(|file_data| {
            let result = verify_file_needs_download(&file_data, dir, hashes);
            verified_file_count = verified_file_count + 1;
            pb_arc.inc(1);

            result
        });

        // "any" skips remaining elements as soon as one element hits
        // add the remaining file count to progress bar
        if verified_file_count < update_data.files.len() as u64 {
            pb_arc.inc(update_data.files.len() as u64 - verified_file_count);
        }

        if any_file_of_archive_needs_download {
            archives_to_download.push(archive);
        }
    }

    pb_arc.finish_and_clear();
    Ok((archives_to_download, files_to_download))
}

fn verify_downloaded_file(
    file_path: &Path,
    expected_hash: &str,
    file_name: &str,
) -> Result<bool, Box<dyn std::error::Error>> {
    match file_path.get_blake3() {
        Ok(local_hash) => {
            if local_hash.to_lowercase() == expected_hash.to_lowercase() {
                log::info!("Successfully downloaded and verified {file_name}");
                Ok(true)
            } else {
                log::error!(
                    "Hash verification failed for {file_name}: expected {expected_hash}, got {local_hash}"
                );
                Ok(false)
            }
        }
        Err(e) => {
            log::error!("Failed to calculate hash for downloaded file {file_name}: {e}");
            Err(e.into())
        }
    }
}

fn setup_progress_bars(total_size: u64) -> (ProgressBar, ProgressBar) {
    let multi_progress = indicatif::MultiProgress::new();

    let file_progress = ProgressBar::new(0);
    let file_style = indicatif::ProgressStyle::with_template(
        "{spinner:.white} {wide_msg:!.green.bold}  {bytes:>10} / {total_bytes:>10} {percent:>3}%  {bytes_per_sec:>10}  {eta_precise}",
    );
    file_progress.set_style(file_style.unwrap());

    let total_progress = ProgressBar::new(total_size);
    let total_style = indicatif::ProgressStyle::with_template(
        "[{bar:50.green/white}] {wide_msg:!.green/white}  {bytes:>10} / {total_bytes:>10} {percent:>3}%  {bytes_per_sec:>10}  {eta_precise}",
    ).unwrap().progress_chars("■■□");
    total_progress.set_style(total_style);

    let file_progress = multi_progress.add(file_progress);
    let total_progress = multi_progress.add(total_progress);

    (file_progress, total_progress)
}

fn ensure_parent_dir_exists(path: &PathBuf) -> Result<(), Box<dyn std::error::Error>> {
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent).map_err(|e| {
            let error_msg = format!("Failed to create directory {}: {}", parent.cute_path(), e);
            log::error!("{error_msg}");
            error_msg
        })?;
    }

    Ok(())
}

async fn download_file_to_disk(
    url: &str,
    target_path: &PathBuf,
    update_file_data: &UpdateFileData,
    hashes: &mut std::collections::HashMap<String, String>,
    file_pb: &ProgressBar,
    total_pb: &ProgressBar,
    cumulative_downloaded: &mut u64,
) -> Result<(), Box<dyn std::error::Error>> {
    let mut download_successful = false;
    let mut attempts = 0;

    while !download_successful && attempts < MAX_DOWNLOAD_ATTEMPTS {
        attempts += 1;
        log::debug!(
            "Download attempt {} for {}",
            attempts,
            update_file_data.path
        );

        let url_with_cache_bust = if attempts > 1 {
            format!("{}?{}", url, misc::random_string(10))
        } else {
            url.to_string()
        };

        file_pb.set_length(update_file_data.size as u64);
        file_pb.set_position(0);

        match http::download_file_progress(
            file_pb,
            total_pb,
            &url_with_cache_bust,
            target_path,
            update_file_data.size as u64,
            *cumulative_downloaded,
            &update_file_data.path,
        )
        .await
        {
            Ok(_) => {
                log::debug!(
                    "Download completed for {}, verifying hash",
                    update_file_data.path
                );
                match verify_downloaded_file(
                    target_path,
                    &update_file_data.blake3,
                    &update_file_data.path,
                )? {
                    true => {
                        hashes.insert(
                            update_file_data.path.to_string(),
                            update_file_data.blake3.to_lowercase(),
                        );
                        download_successful = true;
                        *cumulative_downloaded += update_file_data.size as u64;
                    }
                    false => {
                        if attempts < MAX_DOWNLOAD_ATTEMPTS {
                            log::info!("Waiting {RETRY_DELAY_SECONDS} seconds before retry..");
                            tokio::time::sleep(tokio::time::Duration::from_secs(
                                RETRY_DELAY_SECONDS,
                            ))
                            .await;
                        }
                    }
                }
            }
            Err(e) => {
                log::error!(
                    "Download attempt {} failed for {}: {}",
                    attempts,
                    update_file_data.path,
                    e
                );
            }
        }
    }

    if !download_successful {
        let error_msg = format!(
            "Failed to download {} after {} attempts",
            update_file_data.path, MAX_DOWNLOAD_ATTEMPTS
        );
        log::error!("{error_msg}");
        return Err(error_msg.into());
    }

    Ok(())
}

fn extract_archive(
    archive_path: &PathBuf,
    install_path: &Path,
    archive: &UpdateArchive,
) -> Result<(), Box<dyn std::error::Error>> {
    println_info!("Extracting archive {}", archive.file_data.path);

    let file = File::open(archive_path)?;
    let mut buf_reader = BufReader::new(file);
    let mut zip = ZipArchive::new(&mut buf_reader)?;

    for archive_file in archive.files.iter() {
        let extract_file_path = install_path.join(&archive_file.path);
        if fs::exists(&extract_file_path)?
            && verify_downloaded_file(&extract_file_path, &archive_file.blake3, &archive_file.path)?
        {
            info!(
                "File {} from archive {} is already up to date!",
                archive_file.path, archive.file_data.path
            );
            continue;
        }

        match zip.by_name(&archive_file.path) {
            Ok(mut zip_file) => {
                ensure_parent_dir_exists(&extract_file_path)?;

                let mut file = File::options()
                    .create(true)
                    .truncate(true)
                    .write(true)
                    .open(extract_file_path)?;
                std::io::copy(&mut zip_file, &mut file)?;

                info!(
                    "Extracted file {} from archive {}",
                    archive_file.path, archive.file_data.path
                )
            }
            Err(_) => {
                let message = format!(
                    "Could not find file {} in archive {}",
                    archive_file.path, archive.file_data.path
                );
                log::error!("{message}");
                crate::println_error!("{message}");
                return Err(Box::from(message));
            }
        }
    }

    Ok(())
}

/// Verifies files whether they are outdated and afterward attempts to download any outdated ones.
async fn download_files(
    update_data: &UpdateData,
    install_path: &Path,
    launcher_dir: &Path,
    hashes: &mut std::collections::HashMap<String, String>,
) -> Result<(), Box<dyn std::error::Error>> {
    let (archives_to_download, files_to_download) =
        verify_files(update_data, install_path, hashes)?;
    if archives_to_download.is_empty() && files_to_download.is_empty() {
        log::info!("All files are up to date");
        crate::println_info!("No update required - all files are up to date");
        return Ok(());
    }

    let total_size = files_to_download
        .iter()
        .map(|f| f.file_data.size as u64)
        .chain(archives_to_download.iter().map(|a| a.file_data.size as u64))
        .sum::<u64>();
    let total_download_count = files_to_download.len() + archives_to_download.len();

    log::info!(
        "Need to download {} files ({} total)",
        total_download_count,
        misc::human_readable_bytes(total_size)
    );
    crate::println_info!(
        "Update required - downloading {} files ({})",
        total_download_count,
        misc::human_readable_bytes(total_size)
    );

    let (file_pb, total_pb) = setup_progress_bars(total_size);
    let mut cumulative_downloaded = 0u64;

    for (file_idx, file) in files_to_download.iter().enumerate() {
        let file_path = install_path.join(&file.file_data.path);
        ensure_parent_dir_exists(&file_path)?;

        log::info!(
            "Downloading file {}/{}: {} from {}",
            file_idx + 1,
            total_download_count,
            file.file_data.path,
            file.url
        );

        download_file_to_disk(
            file.url.as_str(),
            &file_path,
            &file.file_data,
            hashes,
            &file_pb,
            &total_pb,
            &mut cumulative_downloaded,
        )
        .await?;
    }

    for (archive_idx, archive) in archives_to_download.iter().enumerate() {
        let archive_download_path = launcher_dir.join(&archive.file_data.path);
        ensure_parent_dir_exists(&archive_download_path)?;

        log::info!(
            "Downloading archive {}/{}: {} from {}",
            archive_idx + 1,
            total_download_count,
            archive.file_data.path,
            archive.url
        );

        if !fs::exists(&archive_download_path)?
            || !verify_downloaded_file(
                &archive_download_path,
                &archive.file_data.blake3,
                &archive.file_data.path,
            )?
        {
            download_file_to_disk(
                archive.url.as_str(),
                &archive_download_path,
                &archive.file_data,
                hashes,
                &file_pb,
                &total_pb,
                &mut cumulative_downloaded,
            )
            .await?;
        } else {
            println_info!("Archive {} already downloaded!", archive.file_data.path);
        }

        extract_archive(&archive_download_path, install_path, archive)?;

        fs::remove_file(&archive_download_path)?;
        println_info!("Removed download artifact {}!", archive.file_data.path);
    }

    file_pb.finish_and_clear();
    total_pb.finish_and_clear();

    Ok(())
}

pub async fn update(
    repo_name: &str,
    update_data: &UpdateData,
    install_path: &Path,
    launcher_dir: &Path,
    hashes: &mut std::collections::HashMap<String, String>,
) -> Result<(), Box<dyn std::error::Error>> {
    println_info!("Checking for updates from {repo_name}");
    log::info!("Starting update process for {repo_name}",);

    download_files(update_data, install_path, launcher_dir, hashes).await?;

    log::info!("Update process finished for {repo_name}",);
    Ok(())
}

pub fn launch_game(dir: &Path, args: &str) -> Result<(), Box<dyn std::error::Error>> {
    let exe_path = dir.join(GAME_EXECUTABLE);
    log::info!("Launching IW4x: {}", exe_path.cute_path());

    if !exe_path.exists() {
        let error_msg = format!("IW4x executable not found: {}", exe_path.cute_path());
        log::error!("{error_msg}");
        return Err(error_msg.into());
    }

    println!("\n\nJoin the IW4x Discord server:\n{DISCORD_INVITE_1}\n{DISCORD_INVITE_2}\n\n");

    crate::println_info!("Launching IW4x {}", args);

    let exit_status: std::process::ExitStatus;
    let launch_args = args.split_whitespace();

    #[cfg(windows)]
    {
        log::info!("Starting IW4x");
        exit_status = std::process::Command::new(&exe_path)
            .args(launch_args)
            .current_dir(dir)
            .spawn()
            .map_err(|e| {
                log::error!("Failed to spawn IW4x process: {e}");
                e
            })?
            .wait()
            .map_err(|e| {
                log::error!("Failed to wait for IW4x process: {e}");
                e
            })?;
    }

    #[cfg(unix)]
    {
        let (launcher, launcher_name) = if misc::is_program_in_path("umu") {
            ("umu", "umu")
        } else if misc::is_program_in_path("wine") {
            ("wine", "wine")
        } else {
            (exe_path.to_str().unwrap(), "directly")
        };

        log::info!("Launching IW4x using {}", launcher_name);
        if launcher_name != "directly" {
            println!(
                "Found {}, launching IW4x using {}.",
                launcher_name, launcher_name
            );
        }
        println!(
            "For best performance, we recommend adding iw4x.exe to Steam to easily use Proton."
        );
        println!(
            "If you run into issues or want to launch a different way, run {} manually.",
            exe_path.cute_path()
        );

        let mut cmd = std::process::Command::new(launcher);
        if launcher_name == "directly" {
            cmd.args(launch_args);
        } else {
            cmd.args([exe_path.to_str().unwrap()]).args(launch_args);
        }

        exit_status = cmd
            .current_dir(dir)
            .spawn()
            .map_err(|e| {
                log::error!("Failed to spawn {} process: {}", launcher_name, e);
                e
            })?
            .wait()
            .map_err(|e| {
                log::error!("Failed to wait for {} process: {}", launcher_name, e);
                e
            })?;
    }

    if exit_status.success() {
        log::info!("IW4x exited successfully with status: {exit_status}");
    } else {
        log::error!("IW4x exited with error status: {exit_status}");
        crate::println_error!("IW4x exited with {exit_status}");
        misc::enter_exit(exit_status.code().unwrap_or(1));
    }

    Ok(())
}
