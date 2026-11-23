package com.denalal.soundpaddashboard.ui.viewmodel

import android.app.Application
import android.util.Log
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.denalal.soundpaddashboard.AppSettings
import com.denalal.soundpaddashboard.data.remote.SoundpadRepository
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import kotlin.coroutines.cancellation.CancellationException

class AppViewModel(application: Application) : AndroidViewModel(application) {
    private val settings = AppSettings(application)
    private val repository = SoundpadRepository()

    private val _uiState = MutableStateFlow(
        AppUiState(
            dark = settings.dark,
            host = settings.host,
            port = settings.port,
            token = settings.token
        )
    )
    val uiState = _uiState.asStateFlow()

    init {
        if (settings.host.isNotBlank()) {
            refresh()
        }
    }

    fun refresh() {
        val state = _uiState.value

        if (state.loading || state.host.isBlank()) return

        viewModelScope.launch {
            _uiState.update {
                it.copy(
                    loading = true,
                    status = ReqStatus.Loading
                )
            }

            try {
                val keys = repository.getKeys(
                    host = state.host,
                    port = state.port,
                    token = state.token
                )

                _uiState.update {
                    it.copy(
                        keys = keys,
                        loading = false,
                        status = ReqStatus.Ok
                    )
                }
            } catch (e: Exception) {
                _uiState.update {
                    it.copy(
                        loading = false,
                        status = ReqStatus.Error
                    )
                }
            }
        }
    }


    fun pressKey(id: String) {
        val state = _uiState.value

        if (state.loading || state.host.isBlank()) return

        viewModelScope.launch {
            try {
                repository.press(
                    host = state.host,
                    port = state.port,
                    token = state.token,
                    id = id
                )
            } catch (e: CancellationException) {
                throw e
            } catch (e: Exception) {
                Log.e("AppViewModel", "Не удалось нажать кнопку", e)
            }
        }
    }

    fun updateHost(value: String) {
        _uiState.update { it.copy(host = value) }
    }

    fun updatePort(value: String) {
        _uiState.update { it.copy(port = value) }
    }

    fun updateToken(value: String) {
        _uiState.update { it.copy(token = value) }
    }

    fun updateDark(value: Boolean) {
        _uiState.update { it.copy(dark = value) }
        settings.dark = value
    }

    fun selectTab(value: Int) {
        _uiState.update { it.copy(selectedTab = value) }
    }

    fun saveSettings() {
        val state = _uiState.value

        settings.host = state.host
        settings.port = state.port
        settings.token = state.token
        settings.dark = state.dark

        selectTab(0)
        refresh()
    }
}