#[derive(serde::Serialize, serde::Deserialize, Clone, Debug)]
pub struct UpdateFileDto {
    pub blake3: String,
    pub size: u32,
    pub path: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub asset_name: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub archive: Option<String>,
}

#[derive(serde::Serialize, serde::Deserialize, Clone, Debug)]
pub struct UpdateArchiveDto {
    pub blake3: String,
    pub size: u32,
    pub name: String,
}

#[derive(serde::Serialize, serde::Deserialize, Clone, Debug)]
pub struct UpdateDataDto {
    pub archives: Vec<UpdateArchiveDto>,
    pub files: Vec<UpdateFileDto>,
}
