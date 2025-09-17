use semver::Version;

pub struct GitHubAsset {
    pub name: String,
    pub url: String,
}

pub struct GitHubRelease {
    pub _repo_owner: String,
    pub repo_name: String,
    pub release_name: String,
    pub assets: Vec<GitHubAsset>,
}

#[derive(serde::Deserialize)]
struct GitHubAssetDto {
    name: String,
    browser_download_url: String,
}

#[derive(serde::Deserialize)]
struct GitHubReleaseDto {
    name: String,
    assets: Vec<GitHubAssetDto>,
}

impl From<GitHubAssetDto> for GitHubAsset {
    fn from(value: GitHubAssetDto) -> Self {
        GitHubAsset {
            name: value.name,
            url: value.browser_download_url,
        }
    }
}

impl GitHubReleaseDto {
    fn into_release(self, repo_owner: String, repo_name: String) -> GitHubRelease {
        GitHubRelease {
            _repo_owner: repo_owner,
            repo_name,
            release_name: self.name,
            assets: self
                .assets
                .into_iter()
                .map(move |asset_dto: GitHubAssetDto| GitHubAsset::from(asset_dto))
                .collect(),
        }
    }
}

pub async fn latest_tag(
    owner: &str,
    repo: &str,
    prerelease: Option<bool>,
) -> Result<GitHubRelease, Box<dyn std::error::Error>> {
    if prerelease.unwrap_or(false) {
        latest_release_prerelease(owner, repo).await
    } else {
        latest_release_full(owner, repo).await
    }
}

pub async fn latest_release_full(
    owner: &str,
    repo: &str,
) -> Result<GitHubRelease, Box<dyn std::error::Error>> {
    let github_body = crate::http::get_body_string(&format!(
        "https://api.github.com/repos/{owner}/{repo}/releases/latest"
    ))
    .await
    .map_err(|e| format!("Failed to fetch GitHub API: {e}"))?;

    let latest_release_dto: GitHubReleaseDto = serde_json::from_str(&github_body)
        .map_err(|e| format!("Failed to parse GitHub API response: {e}"))?;

    Ok(latest_release_dto.into_release(owner.to_string(), repo.to_string()))
}

pub async fn latest_release_prerelease(
    owner: &str,
    repo: &str,
) -> Result<GitHubRelease, Box<dyn std::error::Error>> {
    let github_body = crate::http::get_body_string(&format!(
        "https://api.github.com/repos/{owner}/{repo}/releases"
    ))
    .await
    .map_err(|e| format!("Failed to fetch GitHub API: {e}"))?;

    let github_json: Vec<GitHubReleaseDto> = serde_json::from_str(&github_body)
        .map_err(|e| format!("Failed to parse GitHub API response: {e}"))?;

    let latest_release_dto = github_json.into_iter().next().ok_or("No releases found")?;

    Ok(latest_release_dto.into_release(owner.to_string(), repo.to_string()))
}

pub async fn latest_version(
    owner: &str,
    repo: &str,
    prerelease: Option<bool>,
) -> Result<Version, Box<dyn std::error::Error>> {
    let release_name = latest_tag(owner, repo, prerelease).await?.release_name;
    let cleaned_release_name = release_name.replace('v', "");
    Version::parse(&cleaned_release_name)
        .map_err(|e| format!("Failed to parse version '{cleaned_release_name}': {e}").into())
}

pub fn download_url(owner: &str, repo: &str, tag: Option<&str>) -> String {
    if let Some(tag) = tag {
        format!("https://github.com/{owner}/{repo}/releases/download/{tag}")
    } else {
        format!("https://github.com/{owner}/{repo}/releases/latest")
    }
}
