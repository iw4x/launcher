use semver::Version;

use crate::{github, global::*};

pub async fn self_update_available(prerelease: Option<bool>) -> bool {
    let current_version = match Version::parse(env!("CARGO_PKG_VERSION")) {
        Ok(v) => v,
        Err(e) => {
            log::error!("Failed to parse current version: {e}");
            return false;
        }
    };

    let latest_version = match github::latest_version(GH_OWNER, GH_REPO_LAUNCHER, prerelease).await
    {
        Ok(v) => v,
        Err(e) => {
            log::error!("Failed to get latest version: {e}");
            return false;
        }
    };

    current_version < latest_version
}

#[cfg(not(windows))]
pub async fn run(_update_only: bool, _prerelease: Option<bool>) {
    if self_update_available(None).await {
        crate::println_info!("A new version of the IW4x launcher is available.");
        crate::println_info!(
            "Download it at {}",
            github::download_url(GH_OWNER, GH_REPO, None)
        );
        println!("Launching in 10 seconds..");
        tokio::time::sleep(tokio::time::Duration::from_secs(10)).await;
    }
}

#[cfg(windows)]
pub fn restart() -> Result<(), std::io::Error> {
    use std::os::windows::process::CommandExt;
    match std::process::Command::new(std::env::current_exe().unwrap())
        .args(std::env::args().skip(1))
        .creation_flags(0x00000010) // CREATE_NEW_CONSOLE
        .spawn()
    {
        Ok(_) => std::process::exit(0),
        Err(err) => Err(err),
    }
}

#[cfg(windows)]
pub async fn run(update_only: bool, prerelease: Option<bool>) {
    use std::{fs, path::PathBuf};

    let working_dir = std::env::current_dir().unwrap();
    let files = fs::read_dir(&working_dir).unwrap();

    log::info!("Cleaning up old launcher files");
    for file in files {
        let file = file.unwrap();
        let file_name = file.file_name().into_string().unwrap();

        if (file_name.contains("iw4x-launcher") || file_name.contains("alterware-launcher"))
            && (file_name.contains(".__relocated__.exe")
                || file_name.contains(".__selfdelete__.exe"))
        {
            match fs::remove_file(file.path()) {
                Ok(_) => log::info!("Removed old launcher file: {file_name}"),
                Err(e) => log::error!("Failed to remove old launcher file {file_name}: {e}"),
            }
        }
    }

    if self_update_available(prerelease).await {
        log::info!("Self-update available, starting update process");
        crate::println_info!("Performing launcher self-update");
        println!(
            "If you run into any issues, please download the latest version at {}",
            github::download_url(GH_OWNER, GH_REPO_LAUNCHER, None)
        );

        let update_binary = PathBuf::from("iw4x-launcher-update.exe");
        let file_path = working_dir.join(&update_binary);

        if update_binary.exists() {
            if let Err(e) = fs::remove_file(&update_binary) {
                log::error!("Failed to remove existing update binary: {e}");
            }
        }

        let launcher_name = "iw4x-launcher.exe";

        let download_url = format!(
            "{}/download/{}",
            github::download_url(GH_OWNER, GH_REPO_LAUNCHER, None),
            launcher_name
        );

        log::info!("Downloading launcher update from: {download_url}");

        if let Err(e) = crate::http::download_file(&download_url, &file_path).await {
            log::error!("Failed to download launcher update: {e}");
            crate::println_error!("Failed to download launcher update.");
            return;
        }

        if !file_path.exists() {
            log::error!("Update file does not exist after download");
            crate::println_error!("Failed to download launcher update.");
            return;
        }

        log::info!("Replacing current executable with update");
        if let Err(e) = self_replace::self_replace("iw4x-launcher-update.exe") {
            log::error!("Failed to replace executable: {e}");
            crate::println_error!("Failed to replace launcher executable.");
            return;
        }

        if let Err(e) = fs::remove_file(&file_path) {
            log::warn!("Failed to cleanup update file: {e}");
        }

        // restarting spawns a new console, automation should manually restart on exit code 201
        if !update_only {
            if let Err(e) = restart() {
                let restart_error = e.to_string();
                log::error!("Failed to restart launcher: {restart_error}");
                crate::println_error!("Failed to restart launcher: {restart_error}");
                println!("Please restart the launcher manually.");
                crate::misc::enter_exit(201);
            }
        }
        log::info!("Self-update completed, exiting with code 201");
        std::process::exit(201);
    } else {
        log::info!("No self-update available");
    }
}
