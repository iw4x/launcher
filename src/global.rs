use std::time::Duration;

use once_cell::sync::Lazy;

pub const GH_OWNER: &str = "iw4x";
pub const GH_REPO_LAUNCHER: &str = "launcher";
pub const GH_REPO_RAW_FILES: &str = "iw4x-rawfiles";
pub const GH_REPO_CLIENT: &str = "iw4x-client";

#[cfg(windows)]
pub const DESKTOP_SHORTCUT_NAME: &str = "IW4x.lnk";
pub const GAME_EXECUTABLE: &str = "iw4x.exe";
pub const LAUNCHER_DIR: &str = "launcher";

pub const UPDATE_INFO_ASSET_NAME: &str = "update.json";

pub const MAX_DOWNLOAD_ATTEMPTS: usize = 2;
pub const RETRY_DELAY_SECONDS: u64 = 5;
pub const HTTP_TIMEOUT: Duration = Duration::from_secs(5);

pub const DISCORD_INVITE_1: &str = "https://iw4x.io/discord";
pub const DISCORD_INVITE_2: &str = "https://discord.com/invite/pV2qJscTXf";

pub const INSTALL_GUIDE: &str = "https://iw4x.io/install";

pub const LOG_TIME_FORMAT: &str = "%Y-%m-%d %H:%M:%S.%f";
pub const LOG_LEVEL: &str = "debug";

pub static USER_AGENT: Lazy<String> = Lazy::new(|| {
    format!(
        "IW4x Launcher v{} on {} | github.com/{}/{}",
        env!("CARGO_PKG_VERSION"),
        std::env::consts::OS,
        GH_OWNER,
        GH_REPO_LAUNCHER
    )
});
