package com.denalal.soundpaddashboard.data.model

import kotlinx.serialization.Serializable

@Serializable
data class PressResponse(val ok: Boolean, val combo: String)