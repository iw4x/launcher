use crate::extend::CutePath;
use crate::github::GitHubRelease;
use crate::global::UPDATE_INFO_ASSET_NAME;
use crate::release_definition::{UpdateArchiveDto, UpdateDataDto, UpdateFileDto};
use crate::LAUNCHER_DIR;
use crate::{github, http};
use std::fs;
use std::path::{Path, PathBuf};

#[derive(Clone, Debug)]
pub struct UpdateFileData {
    pub blake3: String,
    pub size: u32,
    pub path: String,
}

#[derive(Clone, Debug)]
pub struct UpdateArchive {
    pub file_data: UpdateFileData,
    pub url: String,
    pub files: Vec<UpdateFileData>,
}

#[derive(Clone, Debug)]
pub struct UpdateFile {
    pub file_data: UpdateFileData,
    pub url: String,
}

#[derive(Clone, Debug)]
pub struct UpdateData {
    pub archives: Vec<UpdateArchive>,
    pub files: Vec<UpdateFile>,
}

impl UpdateArchiveDto {
    fn into_domain(
        self,
        release: &GitHubRelease,
    ) -> Result<UpdateArchive, Box<dyn std::error::Error>> {
        let url = release
            .assets
            .iter()
            .find(|asset| asset.name == self.name)
            .ok_or_else(|| {
                let error_message = format!(
                    "Archive {} from definition is missing its asset in release {}",
                    self.name, release.release_name
                );
                log::error!("{error_message}");
                error_message
            })?
            .url
            .clone();

        Ok(UpdateArchive {
            file_data: UpdateFileData {
                blake3: self.blake3,
                path: self.name,
                size: self.size,
            },
            files: vec![],
            url,
        })
    }
}

impl UpdateFileDto {
    fn add_to_update_data(
        self,
        update_data: &mut UpdateData,
        release: &GitHubRelease,
    ) -> Result<(), Box<dyn std::error::Error>> {
        if let Some(archive_name) = self.archive {
            let found_archive = update_data
                .archives
                .iter_mut()
                .find(|archive| archive.file_data.path == archive_name)
                .ok_or_else(|| {
                    let error_message = format!(
                        "Archive {archive_name} of file {} could not be found",
                        self.path
                    );
                    log::error!("{error_message}");
                    error_message
                })?;

            found_archive.files.push(UpdateFileData {
                blake3: self.blake3,
                path: self.path,
                size: self.size,
            })
        } else if let Some(asset_name) = self.asset_name {
            let found_asset = release
                .assets
                .iter()
                .find(|release| release.name == asset_name)
                .ok_or_else(|| {
                    let error_message = format!(
                        "Asset {asset_name} of file {} could not be found",
                        self.path
                    );
                    log::error!("{error_message}");
                    error_message
                })?;

            update_data.files.push(UpdateFile {
                file_data: UpdateFileData {
                    blake3: self.blake3,
                    path: self.path,
                    size: self.size,
                },
                url: found_asset.url.clone(),
            });
        }

        Ok(())
    }
}

impl UpdateDataDto {
    fn into_domain(
        self,
        release: &GitHubRelease,
    ) -> Result<UpdateData, Box<dyn std::error::Error>> {
        let maybe_archives: Result<Vec<UpdateArchive>, Box<dyn std::error::Error>> = self
            .archives
            .into_iter()
            .map(|archive_dto| archive_dto.into_domain(release))
            .collect();
        let archives = maybe_archives?;

        let mut update_data = UpdateData {
            archives,
            files: vec![],
        };

        self.files
            .into_iter()
            .try_for_each(|file| file.add_to_update_data(&mut update_data, release))?;

        Ok(update_data)
    }
}

pub const REQUIRED_GAME_FILES: [&str; 2] = ["binkw32.dll", "mss32.dll"];

pub fn required_files_exist(dir: &Path) -> bool {
    for required_file in REQUIRED_GAME_FILES {
        let file_path = dir.join(required_file);
        if !file_path.exists() {
            crate::println_error!("Required file {} does not exist", file_path.cute_path());
            return false;
        }
    }
    true
}

async fn fetch_definition_data_from_release(
    release: &GitHubRelease,
) -> Result<String, Box<dyn std::error::Error>> {
    let definition_asset = release
        .assets
        .iter()
        .find(|&a| a.name == UPDATE_INFO_ASSET_NAME)
        .ok_or_else(|| {
            let error_message = format!(
                "Release {} is missing its definition file {UPDATE_INFO_ASSET_NAME}",
                release.release_name
            );
            log::error!("{error_message}");
            error_message
        })?;

    http::get_body_string(&definition_asset.url)
        .await
        .map_err(|e| {
            log::error!(
                "Failed to fetch game data from {}: {e}",
                definition_asset.url
            );
            Box::from(format!("Failed to fetch game data: {e}"))
        })
}

#[cfg(debug_assertions)]
async fn fetch_definition_data_from_disk(release: &GitHubRelease) -> Option<String> {
    let disk_definition_path = PathBuf::from(format!("{LAUNCHER_DIR}/{}.json", release.repo_name));
    if !disk_definition_path.exists() {
        return None;
    }

    fs::read_to_string(&disk_definition_path).ok()
}

async fn update_definition_from_release(
    release: &GitHubRelease,
) -> Result<UpdateData, Box<dyn std::error::Error>> {
    let raw_data: String;

    #[cfg(debug_assertions)]
    {
        raw_data = if let Some(disk_file_data) = fetch_definition_data_from_disk(release).await {
            disk_file_data
        } else {
            fetch_definition_data_from_release(release).await?
        };
    }
    #[cfg(not(debug_assertions))]
    {
        raw_data = fetch_definition_data_from_release(release).await?;
    }

    let update_data: UpdateDataDto = serde_json::from_str(&raw_data).map_err(|e| {
        log::error!("Failed to parse game data JSON: {e}");
        format!("Failed to parse game data: {e}")
    })?;

    update_data.into_domain(release)
}

pub async fn fetch_release_update_data(
    owner: &str,
    repo: &str,
) -> Result<UpdateData, Box<dyn std::error::Error>> {
    let release = github::latest_release_full(owner, repo).await?;
    let definition = update_definition_from_release(&release).await?;

    log::info!(
        "Successfully loaded {owner}/{repo} data with {} files and {} archives",
        definition.files.len(),
        definition.archives.len()
    );

    Ok(definition)
}
