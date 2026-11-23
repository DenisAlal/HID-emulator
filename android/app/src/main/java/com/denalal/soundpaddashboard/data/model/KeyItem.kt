package com.denalal.soundpaddashboard.data.model

import kotlinx.serialization.Serializable

@Serializable
data class KeyItem(
    val id: String,
    val label: String,
    val combo: String
)
