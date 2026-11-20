package com.denalal.soundpaddashboard.ui.theme

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

private val DarkColors = darkColorScheme(
    primary = AccentDark,
    onPrimary = Color(0xFF0B1B33),
    secondary = Green,
    onSecondary = Color(0xFF07230F),
    background = DarkBg,
    onBackground = DarkText,
    surface = DarkSurface,
    onSurface = DarkText,
    surfaceVariant = DarkCard,
    onSurfaceVariant = DarkText,
    outline = Color(0xFF3A3A3A),
    error = Red,
    onError = Color.White
)

private val LightColors = lightColorScheme(
    primary = Accent,
    onPrimary = Color.White,
    secondary = Color(0xFF1E9E55),
    onSecondary = Color.White,
    background = LightBg,
    onBackground = LightText,
    surface = LightCard,
    onSurface = LightText,
    surfaceVariant = LightCard,
    onSurfaceVariant = LightText,
    outline = Color(0xFFCFD2D8),
    error = Red,
    onError = Color.White
)

@Composable
fun SoundpadDashboardTheme(
    darkTheme: Boolean,
    content: @Composable () -> Unit
) {
    MaterialTheme(
        colorScheme = if (darkTheme) DarkColors else LightColors,
        typography = Typography,
        content = content
    )
}
