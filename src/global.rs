use std::time::Duration;

use once_cell::sync::Lazy;

use crate::cdn::{Region, Server};

pub const GH_OWNER: &str = "iw4x";
pub const GH_REPO: &str = "launcher";

pub const CDN_HOSTS: [Server; 0] = [];

pub const DEFAULT_CDN_URL: &str = "";
pub const IP2ASN_URL: &str = "";

#[cfg(windows)]
pub const DESKTOP_SHORTCUT_NAME: &str = "IW4x.lnk";
pub const GAME_EXECUTABLE: &str = "iw4x.exe";
pub const LAUNCHER_DIR: &str = "launcher";

pub const TESTING_INFO: &str = "testing.json";
pub const STABLE_INFO: &str = "stable.json";

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
        GH_REPO
    )
});
