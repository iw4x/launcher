mod ascii_art;
mod cache;
mod cdn;
mod config;
mod extend;
mod game;
mod github;
mod global;
mod http;
mod migrations;
mod misc;
mod self_update;

use std::{
    env, fs, io,
    path::{Path, PathBuf},
};

use clap::Parser;
use colored::Colorize;
use crossterm::{cursor, execute, terminal};
use simple_log::LogConfigBuilder;

use crate::{extend::CutePath, global::*};

// ignore_errors = true is used to prevent the launcher from exiting if an unknown argument is used
#[derive(Parser, Debug)]
#[command(author, version, about, long_about = None, ignore_errors = true)]
struct Args {
    /// Game install path, usually in steamapps/common/Call of Duty Modern Warfare 2
    #[arg(short, long)]
    path: Option<PathBuf>,

    /// Custom config path, default is <game-path>/launcher/config.json
    #[arg(short, long)]
    config: Option<PathBuf>,

    /// Update only, don't launch IW4x
    #[arg(short, long)]
    update: bool,

    /// Force file re-check
    #[arg(short, long)]
    force: bool,

    /// Arguments passed to the game, default is -stdout
    #[arg(long)]
    args: Option<String>,
    #[arg(long, hide = true)]
    pass: Option<String>,

    /// Don't update the launcher
    #[arg(long = "skip-self-update")]
    skip_self_update: bool,
    #[arg(long = "skip-launcher-update", hide = true)]
    skip_launcher_update: bool,

    /// Don't check for required game files
    #[arg(long)]
    ignore_required_files: bool,

    /// Disable CDN rating and use default CDN
    #[arg(long)]
    skip_connectivity_check: bool,

    /// Run in offline mode
    #[arg(long)]
    offline: bool,

    /// Install from testing branch (IW4x & Launcher)
    #[arg(long)]
    testing: bool,

    /// Rate CDN servers and print results
    #[arg(long)]
    rate: bool,

    /// Specify custom CDN url
    #[arg(long)]
    cdn_url: Option<String>,

    #[arg(long = "art-attack", hide = true)]
    art_attack: bool,

    /// Disable ASCII art
    #[arg(long = "disable-art")]
    disable_art: bool,

    /// Install DXVK for better AMD performance
    #[arg(long)]
    dxvk: bool,
}

fn setup_logging(install_path: &Path) -> Result<(), Box<dyn std::error::Error>> {
    let launcher_dir = install_path.join(global::LAUNCHER_DIR);
    fs::create_dir_all(&launcher_dir)?;

    let log_file = launcher_dir.join("log.log");

    if log_file.exists() {
        fs::remove_file(&log_file)?;
    }

    let log_file_str = log_file
        .to_str()
        .ok_or_else(|| format!("Log file path {} is not valid UTF-8", log_file.display()))?;

    let logger_config = LogConfigBuilder::builder()
        .path(log_file_str)
        .time_format(LOG_TIME_FORMAT)
        .level(LOG_LEVEL)
        .map_err(|e| format!("Failed to configure logger: {}", e))?
        .output_file()
        .build();

    simple_log::new(logger_config)?;
    log::info!("Logging initialized, log file: {}", log_file.cute_path());
    Ok(())
}

#[cfg(windows)]
fn create_desktop_shortcut(launcher_path: &Path) -> Result<(), Box<dyn std::error::Error>> {
    use mslnk::ShellLink;

    if let Ok(desktop_path) = env::var("USERPROFILE") {
        let shortcut_path = PathBuf::from(desktop_path)
            .join("Desktop")
            .join(DESKTOP_SHORTCUT_NAME);

        if !shortcut_path.exists() {
            if let Ok(mut sl) = ShellLink::new(launcher_path) {
                sl.set_icon_location(Some(launcher_path.to_string_lossy().to_string()));

                match sl.create_lnk(&shortcut_path) {
                    Ok(_) => println_info!("Created desktop shortcut: {}", DESKTOP_SHORTCUT_NAME),
                    Err(e) => log::warn!("Failed to create desktop shortcut: {}", e),
                }
            }
        }
    }
    Ok(())
}

fn setup_terminal() -> Result<(), Box<dyn std::error::Error>> {
    let mut stdout = io::stdout();

    execute!(
        stdout,
        terminal::SetTitle(format!("IW4x Launcher v{}", env!("CARGO_PKG_VERSION")))
    )?;
    execute!(stdout, cursor::Hide)?;

    Ok(())
}

fn cleanup_terminal() {
    let mut stdout = io::stdout();
    let _ = execute!(stdout, cursor::Show);
}

async fn run_launcher() -> Result<(), Box<dyn std::error::Error>> {
    let args = Args::parse();

    if args.rate {
        cdn::rate_cdns_and_display().await;
        return Ok(());
    }

    if args.art_attack {
        println!("This is an art attack.");
        println!("THIS is an art attack.");
        println!("THIS IS: ART ATTACK!");
        ascii_art::print_all(true);
        return Ok(());
    }

    let install_path = args.path.clone().unwrap_or_else(|| {
        env::current_dir().unwrap_or_else(|_| {
            log::error!("Failed to get current directory, using fallback");
            PathBuf::from(".")
        })
    });
    let launcher_dir = install_path.join(global::LAUNCHER_DIR);

    fs::create_dir_all(&launcher_dir)?;
    migrations::run(&install_path);
    setup_logging(&install_path)?;

    log::info!("IW4x Launcher v{} starting up", env!("CARGO_PKG_VERSION"));
    log::info!("Command line arguments: {:?}", args);
    log::info!("Using install path: {}", install_path.cute_path());
    log::info!("Launcher directory: {}", launcher_dir.cute_path());

    let config_path = if let Some(custom_config) = &args.config {
        if custom_config.is_absolute() {
            custom_config.clone()
        } else {
            env::current_dir()?.join(custom_config)
        }
    } else {
        launcher_dir.join("config.json")
    };

    if args.config.is_some() {
        log::info!("Using custom config path: {}", config_path.cute_path());
    }

    let _is_first_run = !config_path.exists();
    let mut cfg = config::load(config_path);

    if !args.skip_self_update && !args.skip_launcher_update && !cfg.skip_self_update {
        log::info!("Checking for launcher updates");
        self_update::run(false, Some(args.testing)).await;
    }

    if let Some(cdn_url) = args.cdn_url {
        cfg.cdn_url = cdn_url;
    }
    if args.offline {
        cfg.offline = true;
    }
    if args.testing {
        cfg.testing = true;
    }
    if args.update {
        cfg.update_only = true;
    }
    if args.force {
        cfg.force_update = true;
    }
    if args.disable_art {
        cfg.disable_art = true;
    }
    if args.dxvk {
        cfg.dxvk = true;
    }
    if let Some(game_args) = args.args.or(args.pass) {
        cfg.args = game_args;
    }

    if !cfg.disable_art {
        ascii_art::print_random(true);
    }

    log::warn!("The launcher is currently not able to update due to infrastructure changes.");
    log::warn!("We are working on a solution, sorry for the inconvenience!");
    game::launch_game(&install_path, &cfg.args)
}

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    #[cfg(windows)]
    {
        let _ = colored::control::set_virtual_terminal(true);
    }
    let _ = setup_terminal();

    let result = run_launcher().await;
    if let Err(e) = &result {
        println_error!("{}", e);
        cleanup_terminal();
        misc::enter_exit(1);
    }

    cleanup_terminal();
    result
}
