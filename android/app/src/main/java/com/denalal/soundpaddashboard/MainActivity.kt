package com.denalal.soundpaddashboard

import android.app.Activity
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.imePadding
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TopAppBar
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.saveable.listSaver
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalView
import androidx.compose.ui.text.font.FontWeight
import androidx.core.view.WindowCompat
import com.denalal.soundpaddashboard.ui.HomeScreen
import com.denalal.soundpaddashboard.ui.ReqStatus
import com.denalal.soundpaddashboard.ui.SettingsScreen
import com.denalal.soundpaddashboard.ui.StatusDot
import com.denalal.soundpaddashboard.ui.theme.SoundpadDashboardTheme
import kotlinx.coroutines.launch

private val KeyListSaver = listSaver<List<KeyItem>, String>(
    save = { list -> list.flatMap { listOf(it.id, it.label, it.combo) } },
    restore = { flat -> flat.chunked(3).filter { it.size == 3 }.map { KeyItem(it[0], it[1], it[2]) } }
)

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContent {
            App()
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun App() {
    val context = LocalContext.current
    val settings = remember { AppSettings(context) }

    var dark by rememberSaveable { mutableStateOf(settings.dark) }
    var host by rememberSaveable { mutableStateOf(settings.host) }
    var port by rememberSaveable { mutableStateOf(settings.port) }
    var token by rememberSaveable { mutableStateOf(settings.token) }
    var tab by rememberSaveable { mutableStateOf(0) }

    var keys by rememberSaveable(stateSaver = KeyListSaver) {
        mutableStateOf<List<KeyItem>>(emptyList())
    }
    var status by remember { mutableStateOf(ReqStatus.Idle) }
    var loading by remember { mutableStateOf(false) }

    val scope = rememberCoroutineScope()

    val view = LocalView.current
    LaunchedEffect(dark) {
        val window = (view.context as? Activity)?.window ?: return@LaunchedEffect
        val controller = WindowCompat.getInsetsController(window, view)
        controller.isAppearanceLightStatusBars = !dark
        controller.isAppearanceLightNavigationBars = !dark
    }

    val refresh: () -> Unit = {
        if (!loading) {
            loading = true
            scope.launch {
                val result = Api.fetchKeys(host, port, token)
                loading = false
                if (result == null) {
                    status = ReqStatus.Error
                } else {
                    keys = result
                    status = ReqStatus.Ok
                }
            }
        }
    }

    LaunchedEffect(Unit) {
        if (host.isNotBlank() && keys.isEmpty()) refresh()
    }

    SoundpadDashboardTheme(darkTheme = dark) {
        Scaffold(
            topBar = {
                TopAppBar(
                    title = { Text("Soundpad", fontWeight = FontWeight.SemiBold) },
                    actions = {
                        StatusDot(status)
                        TextButton(onClick = refresh, enabled = !loading && host.isNotBlank()) {
                            Text(if (loading) "..." else "Reload")
                        }
                    },
                    colors = TopAppBarDefaults.topAppBarColors(
                        containerColor = MaterialTheme.colorScheme.surface,
                        titleContentColor = MaterialTheme.colorScheme.onSurface
                    )
                )
            },
            bottomBar = {
                NavigationBar(containerColor = MaterialTheme.colorScheme.surface) {
                    NavigationBarItem(
                        selected = tab == 0,
                        onClick = { tab = 0 },
                        icon = { Text("\u25A6") },
                        label = { Text("Home") }
                    )
                    NavigationBarItem(
                        selected = tab == 1,
                        onClick = { tab = 1 },
                        icon = { Text("\u2699") },
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
                if (tab == 0) {
                    HomeScreen(
                        keys = keys,
                        loading = loading,
                        onRefresh = refresh,
                        onPress = { key ->
                            scope.launch {
                                val ok = Api.press(host, port, token, key.id)
                                status = if (ok) ReqStatus.Ok else ReqStatus.Error
                            }
                        }
                    )
                } else {
                    SettingsScreen(
                        dark = dark,
                        onDarkChange = {
                            dark = it
                            settings.dark = it
                        },
                        host = host,
                        port = port,
                        token = token,
                        onHostChange = { host = it },
                        onPortChange = { port = it },
                        onTokenChange = { token = it },
                        onSave = {
                            settings.host = host
                            settings.port = port
                            settings.token = token
                            settings.dark = dark
                            tab = 0
                            refresh()
                        },
                        onReload = {
                            tab = 0
                            refresh()
                        }
                    )
                }
            }
        }
    }
}
