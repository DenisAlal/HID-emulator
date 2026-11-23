package com.denalal.soundpaddashboard.data.remote

import kotlinx.serialization.json.Json
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.OkHttpClient
import okhttp3.logging.HttpLoggingInterceptor
import retrofit2.Retrofit
import retrofit2.converter.kotlinx.serialization.asConverterFactory
import java.util.concurrent.TimeUnit
import com.denalal.soundpaddashboard.BuildConfig
object NetworkClient {
    private val loggingInterceptor =
        HttpLoggingInterceptor().setLevel(HttpLoggingInterceptor.Level.BODY).apply {
            redactHeader("X-Key")
            level = if (BuildConfig.DEBUG) {
                HttpLoggingInterceptor.Level.BODY
            } else {
                HttpLoggingInterceptor.Level.NONE
            }
        }
    private val client = OkHttpClient.Builder()
        .connectTimeout(5, TimeUnit.SECONDS)
        .readTimeout(5, TimeUnit.SECONDS)
        .writeTimeout(5, TimeUnit.SECONDS).addInterceptor(loggingInterceptor)
        .build()

    fun create(host: String, port: String): SoundpadService {

        val raw = host.trim().trimEnd('/').ifEmpty { "127.0.0.1" }
        val scheme = if (raw.startsWith("https://", true)) "https" else "http"
        val contentType = "application/json".toMediaType()
        val authority = raw.substringAfter("://", raw)

        val alreadyHasPort = authority.count { it == ':' } == 1

        val baseUrl = if (alreadyHasPort) {
            "$scheme://$authority/"
        } else {
            "$scheme://$authority:${port.trim()}/"
        }

        return Retrofit.Builder()
            .baseUrl(baseUrl)
            .client(client)
            .addConverterFactory(Json.asConverterFactory(contentType))
            .build()
            .create(SoundpadService::class.java)
    }
}