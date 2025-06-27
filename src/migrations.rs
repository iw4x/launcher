use std::{fs, path::Path};

use crate::global;

/// does various migration and cleanup tasks related to breaking changes
/// errors are ignored, if it fails it fails - this is not critical
pub fn run(install_path: &Path) {
    let launcher_dir = install_path.join(global::LAUNCHER_DIR);

    // config migration
    let old_config = install_path.join("iw4x-launcher.json");
    let new_config = launcher_dir.join("config.json");
    if old_config.exists() && !new_config.exists() {
        let _ = fs::rename(&old_config, &new_config);
    }

    #[cfg(windows)]
    {
        // old shortcut cleanup
        let old_shortcut = install_path.join("launch-iw4x.lnk");
        if old_shortcut.exists() {
            let _ = fs::remove_file(&old_shortcut);
        }
    }

    // old log cleanup
    let old_log = install_path.join("iw4x-launcher.log");
    if old_log.exists() {
        let _ = fs::remove_file(&old_log);
    }

    // old cache cleanup
    let cache_file = install_path.join("iw4x-cache.json");
    if cache_file.exists() {
        let _ = fs::remove_file(&cache_file);
    }

    // alterware cleanup
    let alterware_launcher = [
        "alterware-launcher",
        "alterware-launcher.exe",
        "alterware-launcher-x86.exe",
    ];
    let alterware_exists = alterware_launcher
        .iter()
        .any(|file| install_path.join(file).exists());
    if !alterware_exists {
        let cleanup_files = [
            "awcache.json",
            "alterware-launcher.log",
            "alterware-launcher.json",
        ];

        for file in cleanup_files {
            let file_path = install_path.join(file);
            if file_path.exists() {
                let _ = fs::remove_file(&file_path);
            }
        }
    }
}
