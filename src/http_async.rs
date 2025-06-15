use std::cmp::min;
use std::fs::File;
use std::io::Write;
use std::path::PathBuf;

use futures_util::StreamExt;
use indicatif::ProgressBar;
use reqwest::Client;

use crate::extend::*;
use crate::misc;

pub async fn download_file_progress(
    client: &Client,
    pb: &ProgressBar,
    url: &str,
    path: &PathBuf,
    size: u64,
) -> Result<(), String> {
    debug!("Starting download: {} -> {}", url, path.display());

    let res = client
        .get(url)
        .header(
            "User-Agent",
            format!(
                "IW4x Launcher | github.com/{}/{}",
                crate::global::GH_OWNER,
                crate::global::GH_REPO
            ),
        )
        .send()
        .await
        .map_err(|e| {
            error!("Failed to GET from '{}': {}", url, e);
            format!("Failed to GET from '{url}'")
        })?;

    let total_size = res.content_length().unwrap_or(size);
    debug!("Download size: {}", misc::human_readable_bytes(total_size));
    pb.set_length(total_size);

    let msg = format!(
        "{}{} ({})",
        misc::prefix("downloading"),
        path.cute_path(),
        misc::human_readable_bytes(total_size)
    );
    pb.println(&msg);
    info!("{msg}");
    pb.set_message(path.file_name().unwrap().to_str().unwrap().to_string());

    let mut file =
        File::create(path).map_err(|_| format!("Failed to create file '{}'", path.display()))?;
    let mut downloaded: u64 = 0;
    let mut stream = res.bytes_stream();

    while let Some(item) = stream.next().await {
        let chunk = item.map_err(|e| format!("Error while downloading file: {e}"))?;
        file.write_all(&chunk)
            .map_err(|e| format!("Error while writing to file: {e}"))?;

        downloaded = min(downloaded + (chunk.len() as u64), total_size);
        pb.set_position(downloaded);
    }

    pb.set_message(String::default());
    Ok(())
}

pub async fn download_file(url: &str, path: &PathBuf) -> Result<(), String> {
    let body = get_body(url).await?;
    let mut file = File::create(path).or(Err("Failed to create file"))?;
    file.write_all(&body).or(Err("Failed to write to file"))?;
    Ok(())
}

pub async fn get_body(url: &str) -> Result<Vec<u8>, String> {
    let client = Client::new();
    let res = client
        .get(url)
        .header(
            "User-Agent",
            format!(
                "IW4x Launcher | github.com/{}/{}",
                crate::global::GH_OWNER,
                crate::global::GH_REPO
            ),
        )
        .send()
        .await
        .map_err(|e| format!("Failed to send request: {}", e))?;

    debug!("{} {url}", res.status());

    res.bytes()
        .await
        .map(|b| b.to_vec())
        .map_err(|e| format!("Failed to get body: {}", e))
}

pub async fn get_body_string(url: &str) -> Result<String, String> {
    let body = get_body(url).await?;
    Ok(String::from_utf8(body).unwrap())
}

pub async fn _get_json<T: serde::de::DeserializeOwned>(url: &str) -> Result<T, String> {
    let client = Client::new();
    let res = client
        .get(url)
        .header(
            "User-Agent",
            format!(
                "IW4x Launcher | github.com/{}/{}",
                crate::global::GH_OWNER,
                crate::global::GH_REPO
            ),
        )
        .header("Accept", "application/json")
        .send()
        .await
        .map_err(|e| format!("Failed to send request: {}", e))?;

    debug!("{} {}", res.status(), url);

    if !res.status().is_success() {
        return Err(format!("Request failed with status: {}", res.status()));
    }

    let body = res
        .bytes()
        .await
        .map_err(|e| format!("Failed to read response body: {}", e))?;

    serde_json::from_slice::<T>(&body).map_err(|e| format!("Failed to parse JSON: {}", e))
}
