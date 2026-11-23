package com.denalal.soundpaddashboard.data.model

import kotlinx.serialization.Serializable

@Serializable
data class KeyItems (
    val keys: List<KeyItem>
)