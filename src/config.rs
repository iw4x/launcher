use std::{fs, path::PathBuf};

use crate::extend::CutePath;

#[derive(serde::Deserialize, serde::Serialize, PartialEq, Debug, Clone)]
pub struct Config {
    #[serde(default)]
    pub update_only: bool,
    #[serde(default)]
    pub skip_self_update: bool,
    #[serde(default)]
    pub force_update: bool,
    #[serde(default = "default_args")]
    pub args: String,
    #[serde(default)]
    pub offline: bool,
    #[serde(default)]
    pub testing: bool,
    #[serde(default)]
    pub disable_art: bool,
    #[serde(default)]
    pub dxvk: bool,
}

fn default_args() -> String {
    "-stdout".to_string()
}

impl Default for Config {
    fn default() -> Self {
        Self {
            update_only: false,
            skip_self_update: false,
            force_update: false,
            args: "-stdout".to_string(),
            offline: false,
            testing: false,
            disable_art: false,
            dxvk: false,
        }
    }
}

pub fn load(config_path: PathBuf) -> Config {
    log::debug!("Loading config from: {}", config_path.cute_path());
    if config_path.exists() {
        let cfg_str = fs::read_to_string(&config_path).unwrap_or_default();
        let cfg: Config = serde_json::from_str(&cfg_str).unwrap_or_else(|e| {
            log::warn!("Failed to parse config file: {}", e);
            log::info!("Creating default config due to parsing error");
            let default_cfg = Config::default();
            save(config_path.clone(), default_cfg.clone());
            default_cfg
        });

        if let Ok(current_json) = serde_json::to_string_pretty(&cfg) {
            if cfg_str.trim() != current_json.trim() {
                log::info!("Adding missing fields to config file");
                save(config_path.clone(), cfg.clone());
            }
        }

        log::debug!("Loaded config: {:?}", cfg);
        cfg
    } else {
        log::info!("No config file found, creating default config");
        let default_cfg = Config::default();
        save(config_path.clone(), default_cfg.clone());
        default_cfg
    }
}

pub fn save(config_path: PathBuf, config: Config) {
    let config_json = match serde_json::to_string_pretty(&config) {
        Ok(json) => json,
        Err(e) => {
            log::error!("Could not serialize config: {}", e);
            return;
        }
    };
    match fs::write(config_path.clone(), config_json) {
        Ok(_) => log::debug!("Saved config to: {}", config_path.cute_path()),
        Err(e) => match e.kind() {
            std::io::ErrorKind::NotFound => {
                if let Some(parent) = config_path.parent() {
                    if fs::create_dir_all(parent).is_ok() {
                        if let Ok(json_config) = serde_json::to_string_pretty(&config) {
                            if let Err(e) = fs::write(config_path, json_config) {
                                crate::println_error!("Error while saving config: {}", e);
                            }
                        }
                    }
                }
            }
            _ => crate::println_error!("Error while saving config: {}", e),
        },
    }
}
