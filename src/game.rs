use std::{fs, path::Path, sync::Arc};

use indicatif::ProgressBar;

use crate::{extend::*, global::*, http, misc};

#[derive(serde::Deserialize, serde::Serialize, Clone, Debug)]
pub struct CdnFile {
    pub blake3: String,
    pub size: u32,
    pub path: String,
}

#[derive(serde::Deserialize, serde::Serialize, Debug)]
pub struct CdnData {
    pub base_dir: String,
    pub references: Vec<String>,
    pub required: Vec<String>,
    pub delete: Vec<String>,
    pub rename: Vec<(String, String)>,
    pub files: Vec<CdnFile>,
}

impl CdnData {
    pub fn required_files_exist(&self, dir: &Path) -> bool {
        for required_file in &self.required {
            let file_path = dir.join(required_file);
            if !file_path.exists() {
                crate::println_error!("Required file {} does not exist", file_path.cute_path());
                return false;
            }
        }
        true
    }
}

pub async fn fetch_game_data(
    testing: bool,
    cdn_url: &str,
) -> Result<CdnData, Box<dyn std::error::Error>> {
    let config_file = if testing { TESTING_INFO } else { STABLE_INFO };
    let url = format!("{}/{}?{}", cdn_url, config_file, misc::random_string(10));

    log::info!("Fetching game data from: {}", url);
    let response = http::get_body_string(&url).await.map_err(|e| {
        log::error!("Failed to fetch game data from {}: {}", url, e);
        format!("Failed to fetch game data: {}", e)
    })?;

    let cdn_data: CdnData = serde_json::from_str(&response).map_err(|e| {
        log::error!("Failed to parse game data JSON: {}", e);
        format!("Failed to parse game data: {}", e)
    })?;

    log::info!(
        "Successfully loaded game data with {} files from {}",
        cdn_data.files.len(),
        config_file
    );
    Ok(cdn_data)
}

fn process_renames(cdn_data: &CdnData, dir: &Path) -> Result<(), Box<dyn std::error::Error>> {
    if cdn_data.rename.is_empty() {
        return Ok(());
    }

    log::info!("Processing {} file renames", cdn_data.rename.len());

    for (old_path, new_path) in &cdn_data.rename {
        let old_file = dir.join(old_path);
        let new_file = dir.join(new_path);

        if old_file.exists() {
            if let Some(parent) = new_file.parent() {
                if let Err(e) = fs::create_dir_all(parent) {
                    log::error!("Failed to create directory {}: {}", parent.cute_path(), e);
                    continue;
                }
            }
            match fs::rename(&old_file, &new_file) {
                Ok(_) => {
                    log::info!("Renamed {} -> {}", old_path, new_path);
                }
                Err(e) => {
                    log::error!("Failed to rename {} -> {}: {}", old_path, new_path, e);
                }
            }
        }
    }

    Ok(())
}

fn verify_files<'a>(
    cdn_data: &'a CdnData,
    dir: &Path,
    hashes: &mut std::collections::HashMap<String, String>,
) -> Result<Vec<&'a CdnFile>, Box<dyn std::error::Error>> {
    log::info!("Checking {} files for updates", cdn_data.files.len());

    let pb = ProgressBar::new(cdn_data.files.len() as u64);
    let pb_style = indicatif::ProgressStyle::with_template(
        "{spinner:.white} Checking files... {pos:>6} / {len:>6} done ({percent:>3}%)",
    );
    pb.set_style(pb_style.unwrap());

    let pb_arc = Arc::new(pb);
    let mut files_to_download: Vec<&CdnFile> = Vec::new();

    for file in &cdn_data.files {
        let file_path = dir.join(&file.path);
        let needs_download = if !file_path.exists() {
            log::debug!("File {} does not exist, will download", file.path);
            true
        } else {
            let hash_remote = file.blake3.to_lowercase();
            let hash_local = hashes
                .get(&file.path)
                .cloned()
                .unwrap_or_else(|| file_path.get_blake3().unwrap())
                .to_lowercase();

            if hash_local != hash_remote {
                true
            } else {
                log::info!("File {} is up to date", file.path);
                hashes.insert(file.path.clone(), file.blake3.to_lowercase());
                false
            }
        };

        pb_arc.inc(1);

        if needs_download {
            files_to_download.push(file);
        }
    }

    pb_arc.finish_and_clear();
    Ok(files_to_download)
}

fn verify_downloaded_file(
    file_path: &std::path::Path,
    expected_hash: &str,
    file_name: &str,
) -> Result<bool, Box<dyn std::error::Error>> {
    match file_path.get_blake3() {
        Ok(local_hash) => {
            if local_hash.to_lowercase() == expected_hash.to_lowercase() {
                log::info!("Successfully downloaded and verified {}", file_name);
                Ok(true)
            } else {
                log::error!(
                    "Hash verification failed for {}: expected {}, got {}",
                    file_name,
                    expected_hash,
                    local_hash
                );
                Ok(false)
            }
        }
        Err(e) => {
            log::error!(
                "Failed to calculate hash for downloaded file {}: {}",
                file_name,
                e
            );
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

async fn download_files(
    cdn_data: &CdnData,
    dir: &Path,
    cdn_url: &str,
    hashes: &mut std::collections::HashMap<String, String>,
) -> Result<(), Box<dyn std::error::Error>> {
    let files_to_download = verify_files(cdn_data, dir, hashes)?;
    if files_to_download.is_empty() {
        log::info!("All files are up to date");
        crate::println_info!("No update required - all files are up to date");
        return Ok(());
    }

    let total_size = files_to_download.iter().map(|f| f.size as u64).sum::<u64>();
    log::info!(
        "Need to download {} files ({} total)",
        files_to_download.len(),
        misc::human_readable_bytes(total_size)
    );
    crate::println_info!(
        "Update required - downloading {} files ({})",
        files_to_download.len(),
        misc::human_readable_bytes(total_size)
    );

    let (file_pb, total_pb) = setup_progress_bars(total_size);
    let mut cumulative_downloaded = 0u64;

    for (file_idx, file) in files_to_download.iter().enumerate() {
        let file_path = dir.join(&file.path);

        if let Some(parent) = file_path.parent() {
            fs::create_dir_all(parent).map_err(|e| {
                let error_msg = format!("Failed to create directory {}: {}", parent.cute_path(), e);
                log::error!("{}", error_msg);
                error_msg
            })?;
        }

        let download_url = format!("{}/{}/{}", cdn_url, cdn_data.base_dir, file.blake3);
        log::info!(
            "Downloading file {}/{}: {} from {}",
            file_idx + 1,
            files_to_download.len(),
            file.path,
            download_url
        );

        let mut download_successful = false;
        let mut attempts = 0;

        while !download_successful && attempts < MAX_DOWNLOAD_ATTEMPTS {
            attempts += 1;
            log::debug!("Download attempt {} for {}", attempts, file.path);

            let url_with_cache_bust = if attempts > 1 {
                format!("{}?{}", download_url, misc::random_string(10))
            } else {
                download_url.clone()
            };

            file_pb.set_length(file.size as u64);
            file_pb.set_position(0);

            match http::download_file_progress(
                &file_pb,
                &total_pb,
                &url_with_cache_bust,
                &file_path,
                file.size as u64,
                cumulative_downloaded,
                &file.path,
            )
            .await
            {
                Ok(_) => {
                    log::debug!("Download completed for {}, verifying hash", file.path);
                    match verify_downloaded_file(&file_path, &file.blake3, &file.path)? {
                        true => {
                            hashes.insert(file.path.clone(), file.blake3.to_lowercase());
                            download_successful = true;
                            cumulative_downloaded += file.size as u64;
                        }
                        false => {
                            if attempts < MAX_DOWNLOAD_ATTEMPTS {
                                log::info!(
                                    "Waiting {} seconds before retry..",
                                    RETRY_DELAY_SECONDS
                                );
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
                        file.path,
                        e
                    );
                }
            }
        }

        if !download_successful {
            let error_msg = format!(
                "Failed to download {} after {} attempts",
                file.path, MAX_DOWNLOAD_ATTEMPTS
            );
            log::error!("{}", error_msg);
            return Err(error_msg.into());
        }
    }

    file_pb.finish_and_clear();
    total_pb.finish_and_clear();

    Ok(())
}

fn process_deletions(cdn_data: &CdnData, dir: &Path) -> Result<(), Box<dyn std::error::Error>> {
    if cdn_data.delete.is_empty() {
        return Ok(());
    }

    log::info!(
        "Processing {} file/directory deletions",
        cdn_data.delete.len()
    );

    for delete_path in &cdn_data.delete {
        let target_path = dir.join(delete_path);
        if target_path.exists() {
            if target_path.is_file() {
                fs::remove_file(&target_path).map_err(|e| {
                    let error_msg = format!("Failed to delete file {}: {}", delete_path, e);
                    log::error!("{}", error_msg);
                    error_msg
                })?;
                log::info!("Deleted file: {}", delete_path);
            } else if target_path.is_dir() {
                fs::remove_dir_all(&target_path).map_err(|e| {
                    let error_msg = format!("Failed to delete directory {}: {}", delete_path, e);
                    log::error!("{}", error_msg);
                    error_msg
                })?;
                log::info!("Deleted directory: {}", delete_path);
            } else {
                log::warn!(
                    "Path {} exists but is neither a file nor a directory",
                    delete_path
                );
            }
        } else {
            log::debug!(
                "Path {} already doesn't exist, skipping deletion",
                delete_path
            );
        }
    }

    Ok(())
}

pub async fn update(
    cdn_data: &CdnData,
    dir: &Path,
    cdn_url: &str,
    hashes: &mut std::collections::HashMap<String, String>,
) -> Result<(), Box<dyn std::error::Error>> {
    crate::println_info!("Checking for updates");
    log::info!("Starting game update process");

    process_renames(cdn_data, dir)?;
    download_files(cdn_data, dir, cdn_url, hashes).await?;
    process_deletions(cdn_data, dir)?;

    log::info!("Game update completed successfully");
    Ok(())
}

pub fn launch_game(dir: &Path, args: &str) -> Result<(), Box<dyn std::error::Error>> {
    let exe_path = dir.join(GAME_EXECUTABLE);
    log::info!("Launching IW4x: {}", exe_path.cute_path());

    if !exe_path.exists() {
        let error_msg = format!("IW4x executable not found: {}", exe_path.cute_path());
        log::error!("{}", error_msg);
        return Err(error_msg.into());
    }

    println!(
        "\n\nJoin the IW4x Discord server:\n{}\n{} \n\n",
        DISCORD_INVITE_1, DISCORD_INVITE_2
    );

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
                log::error!("Failed to spawn IW4x process: {}", e);
                e
            })?
            .wait()
            .map_err(|e| {
                log::error!("Failed to wait for IW4x process: {}", e);
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
        log::info!("IW4x exited successfully with status: {}", exit_status);
    } else {
        log::error!("IW4x exited with error status: {}", exit_status);
        crate::println_error!("IW4x exited with {}", exit_status);
        misc::enter_exit(exit_status.code().unwrap_or(1));
    }

    Ok(())
}
