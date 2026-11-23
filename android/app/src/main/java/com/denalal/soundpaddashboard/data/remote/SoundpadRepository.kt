package com.denalal.soundpaddashboard.data.remote
import com.denalal.soundpaddashboard.data.model.KeyItem
import com.denalal.soundpaddashboard.data.model.KeyItems
import com.denalal.soundpaddashboard.data.model.PressRequest
import kotlinx.coroutines.suspendCancellableCoroutine
import retrofit2.Call
import retrofit2.Response
import retrofit2.Callback
import kotlin.coroutines.resume
import kotlin.coroutines.resumeWithException

class SoundpadRepository {
    suspend fun getKeys(
        host: String,
        port: String,
        token: String
    ): List<KeyItem> {

        val service = NetworkClient.create(host, port)

        return suspendCancellableCoroutine { continuation ->

            val call = service.getKeys(token)

            continuation.invokeOnCancellation {
                call.cancel()
            }

            call.enqueue(object : Callback<KeyItems> {

                override fun onResponse(
                    call: Call<KeyItems>,
                    response: Response<KeyItems>
                ) {
                    if (!continuation.isActive) return

                    if (response.isSuccessful) {
                        val keys = response.body()?.keys

                        if (keys != null) {
                            continuation.resume(keys)
                        } else {
                            continuation.resumeWithException(
                                Exception("Response body is empty")
                            )
                        }
                    } else {
                        continuation.resumeWithException(
                            Exception("HTTP ${response.code()}")
                        )
                    }
                }

                override fun onFailure(
                    call: Call<KeyItems>,
                    t: Throwable
                ) {
                    if (continuation.isActive) {
                        continuation.resumeWithException(t)
                    }
                }
            })
        }
    }

    suspend fun press(
        host: String,
        port: String,
        token: String,
        id: String
    ) {
        val service = NetworkClient.create(host, port)

        val response = service.press(
            token,
            PressRequest(id)
        )

        if (!response.isSuccessful) {
            throw Exception("HTTP ${response.code()}")
        }
    }
}