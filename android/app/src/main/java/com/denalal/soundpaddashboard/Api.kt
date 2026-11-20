package com.denalal.soundpaddashboard

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.json.JSONObject
import java.net.HttpURLConnection
import java.net.URL

data class KeyItem(
    val id: String,
    val label: String,
    val combo: String
)

/**
 * The whole network layer: GET /keys and POST /press. Nothing else.
 */
object Api {

    suspend fun fetchKeys(host: String, port: String, token: String): List<KeyItem>? =
        withContext(Dispatchers.IO) {
            try {
                val conn = open(host, port, "/keys", "GET", token)
                try {
                    if (conn.responseCode != 200) return@withContext null
                    val text = conn.inputStream.bufferedReader().use { it.readText() }
                    val arr = JSONObject(text).getJSONArray("keys")
                    val out = ArrayList<KeyItem>(arr.length())
                    for (i in 0 until arr.length()) {
                        val o = arr.getJSONObject(i)
                        out.add(
                            KeyItem(
                                id = o.optString("id"),
                                label = o.optString("label"),
                                combo = o.optString("combo")
                            )
                        )
                    }
                    out
                } finally {
                    conn.disconnect()
                }
            } catch (_: Exception) {
                null
            }
        }

    suspend fun press(host: String, port: String, token: String, id: String): Boolean =
        withContext(Dispatchers.IO) {
            try {
                val conn = open(host, port, "/press", "POST", token)
                try {
                    val body = JSONObject().put("id", id).toString()
                    conn.doOutput = true
                    conn.setRequestProperty("Content-Type", "application/json")
                    conn.outputStream.use { it.write(body.toByteArray(Charsets.UTF_8)) }
                    conn.responseCode == 200
                } finally {
                    conn.disconnect()
                }
            } catch (_: Exception) {
                false
            }
        }

    private fun open(
        host: String,
        port: String,
        path: String,
        method: String,
        token: String
    ): HttpURLConnection {
        val url = URL(baseUrl(host, port) + path)
        return (url.openConnection() as HttpURLConnection).apply {
            requestMethod = method
            connectTimeout = 5000
            readTimeout = 5000
            if (token.isNotEmpty()) setRequestProperty("X-Key", token)
        }
    }

    /**
     * Accepts "192.168.1.50", "192.168.1.50:45454" or "http://192.168.1.50:45454".
     * If the host already carries a port, the port field is ignored.
     */
    private fun baseUrl(host: String, port: String): String {
        var raw = host.trim().trimEnd('/')
        if (raw.isEmpty()) raw = "127.0.0.1"

        val lower = raw.lowercase()
        val scheme = if (lower.startsWith("https://")) "https" else "http"
        val authority = raw.substringAfter("://", raw)

        val alreadyHasPort = authority.count { it == ':' } == 1
        return if (alreadyHasPort) "$scheme://$authority"
        else "$scheme://$authority:${port.trim()}"
    }
}
