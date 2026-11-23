package com.denalal.soundpaddashboard.ui.viewmodel

import com.denalal.soundpaddashboard.data.model.KeyItem

enum class ReqStatus {
    Idle,
    Loading,
    Ok,
    Error
}

data class AppUiState(
    val keys: List<KeyItem> = emptyList(),

    val status: ReqStatus = ReqStatus.Idle,
    val loading: Boolean = false,


    val dark: Boolean = false,

    val host: String = "",
    val port: String = "",
    val token: String = "",

    val selectedTab: Int = 0,
)
