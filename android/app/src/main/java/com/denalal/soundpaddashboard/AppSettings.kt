package com.denalal.soundpaddashboard

import android.content.Context
import androidx.core.content.edit

/**
 * Small SharedPreferences wrapper. Only what the app actually needs:
 * server host/port/token and the dark/light choice.
 */
class AppSettings(context: Context) {

    private val prefs = context.applicationContext
        .getSharedPreferences("hid_emulator", Context.MODE_PRIVATE)

    var host: String
        get() = prefs.getString("host", "") ?: ""
        set(value) = prefs.edit { putString("host", value.trim()) }

    var port: String
        get() = prefs.getString("port", "45454") ?: "45454"
        set(value) = prefs.edit { putString("port", value.trim()) }

    var token: String
        get() = prefs.getString("token", "secret") ?: "secret"
        set(value) = prefs.edit { putString("token", value) }

    var dark: Boolean
        get() = prefs.getBoolean("dark", true)
        set(value) = prefs.edit { putBoolean("dark", value) }
}
