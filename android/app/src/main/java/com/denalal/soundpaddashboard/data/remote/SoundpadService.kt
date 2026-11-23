package com.denalal.soundpaddashboard.data.remote

import com.denalal.soundpaddashboard.data.model.KeyItems
import com.denalal.soundpaddashboard.data.model.PressRequest
import com.denalal.soundpaddashboard.data.model.PressResponse
import retrofit2.Call
import retrofit2.Response
import retrofit2.http.Body
import retrofit2.http.GET
import retrofit2.http.Header
import retrofit2.http.POST

interface SoundpadService {
    @GET("keys")
    fun getKeys(
        @Header("X-Key") token: String
    ) : Call<KeyItems>

    @POST("press")
    suspend fun press(
        @Header("X-Key") token: String,
        @Body request: PressRequest
    ) : Response<PressResponse>
}