use crate::{
    extend::{Blake3Path, CutePath},
    http, println_info,
};
use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::path::{Path, PathBuf};
use std::{fmt, fs, io};

#[derive(Debug)]
pub enum DlcError {
    Network {
        operation: String,
        source: Box<dyn std::error::Error + Send + Sync>,
    },

    FileSystem {
        path: PathBuf,
        operation: String,
        source: io::Error,
    },

    Integrity {
        file: String,
        expected: String,
        actual: String,
    },

    Parse {
        context: String,
        source: serde_json::Error,
    },

    DownloadFailed {
        file: String,
        attempts: usize,
    },
}

impl fmt::Display for DlcError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            DlcError::Network { operation, .. } => {
                write!(f, "Network error during {}", operation)
            }
            DlcError::FileSystem {
                path, operation, ..
            } => {
                write!(
                    f,
                    "File system error during {} on path '{}'",
                    operation,
                    path.display()
                )
            }
            DlcError::Integrity {
                file,
                expected,
                actual,
            } => {
                write!(
                    f,
                    "Integrity check failed for '{}': expected {}, got {}",
                    file, expected, actual
                )
            }
            DlcError::Parse { context, .. } => {
                write!(f, "Parse error in {}", context)
            }
            DlcError::DownloadFailed { file, attempts, .. } => {
                write!(
                    f,
                    "Download failed for '{}' after {} attempts",
                    file, attempts
                )
            }
        }
    }
}

impl std::error::Error for DlcError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        match self {
            DlcError::Network { source, .. } => Some(source.as_ref()),
            DlcError::FileSystem { source, .. } => Some(source),
            DlcError::Parse { source, .. } => Some(source),
            _ => None,
        }
    }
}

pub type DlcResult<T> = Result<T, DlcError>;

#[derive(Clone, Debug, Deserialize, Serialize, PartialEq, Eq)]
pub struct DlcFile {
    pub blake3: String,
    pub size: u64,
    pub path: String,
    pub asset_name: String,
}

impl DlcFile {
    pub fn file_type(&self) -> DlcFileType {
        DlcFileType::from_filename(&self.asset_name)
    }

    pub fn cache_key(&self) -> String {
        format!("dlc/{}", self.asset_name)
    }

    pub fn cdn_url(&self, base_url: &str) -> String {
        format!("{}/{}", base_url, self.path)
    }
}

#[derive(Clone, Debug, Deserialize, Serialize)]
pub struct DlcManifest {
    pub files: Vec<DlcFile>,
}

impl DlcManifest {
    pub fn get_outdated_files(
        &self,
        install_path: &Path,
        cache: &HashMap<String, String>,
    ) -> DlcResult<Vec<&DlcFile>> {
        let mut outdated = Vec::new();
        for file in &self.files {
            if Self::file_needs_update(file, install_path, cache)? {
                outdated.push(file);
            }
        }
        Ok(outdated)
    }

    fn file_needs_update(
        file: &DlcFile,
        install_path: &Path,
        cache: &HashMap<String, String>,
    ) -> DlcResult<bool> {
        let file_type = file.file_type();
        let installation_dir = file_type.get_installation_path(install_path);
        let file_path = installation_dir.join(&file.asset_name);
        if !file_path.exists() {
            log::debug!(
                "DLC file {} not found, scheduling download",
                file.asset_name
            );
            return Ok(true);
        }

        let cache_key = file.cache_key();
        let local_hash = if let Some(cached_hash) = cache.get(&cache_key) {
            cached_hash.clone()
        } else {
            file_path.get_blake3().map_err(|e| DlcError::FileSystem {
                path: file_path.clone(),
                operation: "hash calculation".to_string(),
                source: e,
            })?
        };

        let hash_matches = local_hash.eq_ignore_ascii_case(&file.blake3);
        if !hash_matches {
            log::debug!(
                "DLC file {} hash mismatch (local: {}, remote: {}), scheduling download",
                file.asset_name,
                local_hash,
                file.blake3
            );
        } else {
            log::debug!("DLC file {} is up to date", file.asset_name);
        }

        Ok(!hash_matches)
    }
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum DlcFileType {
    FF,
    IWD,
    Unknown(String),
}

impl DlcFileType {
    pub fn from_filename(filename: &str) -> Self {
        match filename.split('.').last() {
            Some("ff") => Self::FF,
            Some("iwd") => Self::IWD,
            Some(ext) => Self::Unknown(ext.to_string()),
            None => Self::Unknown("no-extension".to_string()),
        }
    }

    pub fn get_installation_path(&self, install_base: &Path) -> PathBuf {
        match self {
            Self::FF => install_base.join("zone").join("dlc"),
            Self::IWD => install_base.join("iw4x"),
            Self::Unknown(ext) => {
                log::warn!(
                    "Unknown DLC file extension '{}', using default zone/dlc path",
                    ext
                );
                install_base.join("zone").join("dlc")
            }
        }
    }

    pub fn description(&self) -> &str {
        match self {
            Self::FF => "FF",
            Self::IWD => "IWD",
            Self::Unknown(_) => "Unknown",
        }
    }
}

impl fmt::Display for DlcFileType {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(self.description())
    }
}

#[derive(Clone, Debug)]
pub struct DlcContext {
    pub manifest_url: String,
    pub cdn_base_url: String,
    pub max_retry_attempts: usize,
    pub retry_delay_ms: u64,
}

impl Default for DlcContext {
    fn default() -> Self {
        Self {
            manifest_url: "https://cdn.iw4x.io/update.json".to_string(),
            cdn_base_url: "https://cdn.iw4x.io".to_string(),
            max_retry_attempts: 3,
            retry_delay_ms: 2000,
        }
    }
}

pub struct Dlc {
    ctx: DlcContext,
}

impl Dlc {
    pub fn new() -> Self {
        Self {
            ctx: DlcContext::default(),
        }
    }

    pub async fn update_dlc(
        &self,
        install_path: &Path,
        cache: &mut HashMap<String, String>,
    ) -> DlcResult<()> {
        println_info!("Checking for DLC updates");

        let manifest = self.fetch_manifest().await?;
        let outdated_files = manifest.get_outdated_files(install_path, cache)?;
        if outdated_files.is_empty() {
            println_info!("DLC files are up to date");
            return Ok(());
        }

        self.prepare_directories(install_path, &outdated_files)
            .await?;
        self.download_files(install_path, &outdated_files, cache)
            .await?;

        Ok(())
    }

    async fn fetch_manifest(&self) -> DlcResult<DlcManifest> {
        log::info!("Fetching DLC manifest from CDN");
        let raw_data = http::get_body_string(&self.ctx.manifest_url)
            .await
            .map_err(|e| DlcError::Network {
                operation: "manifest fetch".to_string(),
                source: format!("{}", e).into(),
            })?;

        let manifest =
            serde_json::from_str::<DlcManifest>(&raw_data).map_err(|e| DlcError::Parse {
                context: "DLC manifest".to_string(),
                source: e,
            })?;

        log::info!("Loaded DLC manifest with {} files", manifest.files.len());
        Ok(manifest)
    }

    async fn prepare_directories(&self, install_path: &Path, files: &[&DlcFile]) -> DlcResult<()> {
        let mut directories = std::collections::HashSet::new();

        for file in files {
            let dir = file.file_type().get_installation_path(install_path);
            directories.insert(dir);
        }

        for dir in directories {
            fs::create_dir_all(&dir).map_err(|e| DlcError::FileSystem {
                path: dir.clone(),
                operation: "directory creation".to_string(),
                source: e,
            })?;
        }

        Ok(())
    }

    async fn download_files(
        &self,
        install_path: &Path,
        files: &[&DlcFile],
        cache: &mut HashMap<String, String>,
    ) -> DlcResult<()> {
        let total_size: u64 = files.iter().map(|f| f.size).sum();
        let file_count = files.len();

        println_info!("Downloading {} DLC files", file_count);

        let total_pb = indicatif::ProgressBar::new(total_size);
        let total_style = indicatif::ProgressStyle::with_template(
            "{spinner:.white} Downloading DLC... {bytes:>10} / {total_bytes:>10} ({bytes_per_sec:>12}, ETA {eta:>3})",
        ).unwrap();
        total_pb.set_style(total_style);

        let file_pb = indicatif::ProgressBar::new(0);
        let file_style = indicatif::ProgressStyle::with_template(
            "{spinner:.white} {bytes:>10} / {total_bytes:>10} ({percent:>3}%)",
        )
        .unwrap();
        file_pb.set_style(file_style);

        let mut downloaded_total = 0u64;

        for file in files {
            match self
                .download_single_file(file, install_path, &file_pb, &total_pb, downloaded_total)
                .await
            {
                Ok(hash) => {
                    cache.insert(file.cache_key(), hash);
                }
                Err(e) => {
                    log::error!("Unable to download DLC file {}: {}", file.asset_name, e);
                    file_pb.finish_and_clear();
                    total_pb.finish_and_clear();
                    return Err(e);
                }
            }

            downloaded_total += file.size;
        }

        file_pb.finish_and_clear();
        total_pb.finish_and_clear();

        Ok(())
    }

    async fn download_single_file(
        &self,
        file: &DlcFile,
        install_path: &Path,
        file_pb: &indicatif::ProgressBar,
        total_pb: &indicatif::ProgressBar,
        download_offset: u64,
    ) -> DlcResult<String> {
        let file_type = file.file_type();
        let target_dir = file_type.get_installation_path(install_path);
        let target_path = target_dir.join(&file.asset_name);
        let cdn_url = file.cdn_url(&self.ctx.cdn_base_url);

        file_pb.set_length(file.size);
        file_pb.reset();

        log::debug!(
            "Downloading {} file: {} -> {}",
            file_type,
            cdn_url,
            target_path.cute_path()
        );

        for attempt in 1..=self.ctx.max_retry_attempts {
            match http::download_file_progress(
                file_pb,
                total_pb,
                &cdn_url,
                &target_path,
                file.size,
                download_offset,
                &file.asset_name,
            )
            .await
            {
                Ok(()) => {
                    let actual_hash =
                        target_path.get_blake3().map_err(|e| DlcError::FileSystem {
                            path: target_path.clone(),
                            operation: "hash verification".to_string(),
                            source: e,
                        })?;
                    if actual_hash.eq_ignore_ascii_case(&file.blake3) {
                        return Ok(actual_hash);
                    } else {
                        let error = DlcError::Integrity {
                            file: file.asset_name.clone(),
                            expected: file.blake3.clone(),
                            actual: actual_hash,
                        };
                        log::warn!("Attempt {}: {}", attempt, error);
                        let _ = fs::remove_file(&target_path);
                    }
                }
                Err(e) => {
                    log::warn!("Attempt {}: Download failed: {}", attempt, e);
                }
            }

            if attempt < self.ctx.max_retry_attempts {
                tokio::time::sleep(std::time::Duration::from_millis(self.ctx.retry_delay_ms)).await;
            }
        }

        Err(DlcError::DownloadFailed {
            file: file.asset_name.clone(),
            attempts: self.ctx.max_retry_attempts,
        })
    }
}

impl Default for Dlc {
    fn default() -> Self {
        Self::new()
    }
}

pub async fn update_dlc(install_path: &Path, cache: &mut HashMap<String, String>) -> DlcResult<()> {
    let dlc = Dlc::new();
    dlc.update_dlc(install_path, cache).await
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_file_type_detection() {
        assert_eq!(DlcFileType::from_filename("test.ff"), DlcFileType::FF);
        assert_eq!(DlcFileType::from_filename("test.iwd"), DlcFileType::IWD);
        assert!(matches!(
            DlcFileType::from_filename("test.unknown"),
            DlcFileType::Unknown(_)
        ));
    }

    #[test]
    fn test_cache_key_generation() {
        let file = DlcFile {
            blake3: "test123".to_string(),
            size: 1000,
            path: "path/to/file".to_string(),
            asset_name: "test.ff".to_string(),
        };

        assert_eq!(file.cache_key(), "dlc/test.ff");
    }

    #[test]
    fn test_cdn_url_generation() {
        let file = DlcFile {
            blake3: "test123".to_string(),
            size: 1000,
            path: "iw3/zone/dlc/mp_convoy_load.ff".to_string(),
            asset_name: "mp_convoy_load.ff".to_string(),
        };

        let url = file.cdn_url("https://cdn.iw4x.io");
        assert_eq!(url, "https://cdn.iw4x.io/iw3/zone/dlc/mp_convoy_load.ff");
    }

    #[test]
    fn test_installation_paths() {
        let base = Path::new("/game");

        let ff_path = DlcFileType::FF.get_installation_path(base);
        assert_eq!(ff_path, base.join("zone").join("dlc"));

        let iwd_path = DlcFileType::IWD.get_installation_path(base);
        assert_eq!(iwd_path, base.join("iw4x"));
    }
}
