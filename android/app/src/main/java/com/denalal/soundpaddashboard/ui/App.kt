package com.denalal.soundpaddashboard.ui

import android.app.Activity
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.imePadding
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalView
import androidx.core.view.WindowCompat
import com.denalal.soundpaddashboard.ui.common.StatusDot
import com.denalal.soundpaddashboard.ui.home.HomeScreen
import com.denalal.soundpaddashboard.ui.settings.SettingsScreen
import com.denalal.soundpaddashboard.ui.theme.SoundpadDashboardTheme
import com.denalal.soundpaddashboard.ui.viewmodel.AppViewModel

@Composable
fun App(viewModel: AppViewModel) {

    val state by viewModel.uiState.collectAsState()

    val view = LocalView.current

    LaunchedEffect(state.dark) {
        val window = (view.context as? Activity)?.window
            ?: return@LaunchedEffect

        val controller = WindowCompat.getInsetsController(window, view)

        controller.isAppearanceLightStatusBars = !state.dark
        controller.isAppearanceLightNavigationBars = !state.dark
    }

    SoundpadDashboardTheme(darkTheme = state.dark) {

        Scaffold(
            topBar = {
                TopAppBar(
                    title = {
                        Text("Soundpad")
                    },
                    actions = {
                        StatusDot(state.status)

                        TextButton(
                            onClick = viewModel::refresh,
                            enabled = !state.loading && state.host.isNotBlank()
                        ) {
                            Text(
                                if (state.loading) "..." else "Reload"
                            )
                        }
                    }
                )
            },

            bottomBar = {
                NavigationBar {
                    NavigationBarItem(
                        selected = state.selectedTab == 0,
                        onClick = { viewModel.selectTab(0) },
                        icon = { Text("▦") },
                        label = { Text("Home") }
                    )

                    NavigationBarItem(
                        selected = state.selectedTab == 1,
                        onClick = { viewModel.selectTab(1) },
                        icon = { Text("⚙") },
                        label = { Text("Settings") }
                    )
                }
            }

        ) { innerPadding ->

            Box(
                Modifier
                    .fillMaxSize()
                    .padding(innerPadding)
                    .imePadding()
            ) {

                if (state.selectedTab == 0) {

                    HomeScreen(
                        keys = state.keys,
                        loading = state.loading,
                        onRefresh = viewModel::refresh,
                        onPress = { key ->
                            viewModel.pressKey(key.id)
                        }
                    )

                } else {

                    SettingsScreen(
                        dark = state.dark,
                        onDarkChange = viewModel::updateDark,

                        host = state.host,
                        port = state.port,
                        token = state.token,

                        onHostChange = viewModel::updateHost,
                        onPortChange = viewModel::updatePort,
                        onTokenChange = viewModel::updateToken,

                        onSave = viewModel::saveSettings,

                        onReload = {
                            viewModel.selectTab(0)
                            viewModel.refresh()
                        }
                    )
                }
            }
        }
    }
}