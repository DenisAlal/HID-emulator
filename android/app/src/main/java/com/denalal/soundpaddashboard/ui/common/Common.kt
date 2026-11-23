package com.denalal.soundpaddashboard.ui.common

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.denalal.soundpaddashboard.ui.theme.Green
import com.denalal.soundpaddashboard.ui.theme.Blue
import com.denalal.soundpaddashboard.ui.theme.Red
import com.denalal.soundpaddashboard.ui.viewmodel.ReqStatus


@Composable
fun StatusDot(status: ReqStatus, modifier: Modifier = Modifier) {
    val color = when (status) {
        ReqStatus.Ok -> Green
        ReqStatus.Loading -> Blue
        ReqStatus.Error -> Red
        ReqStatus.Idle -> MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.35f)
    }
    Box(
        modifier
            .padding(end = 6.dp)
            .size(10.dp)
            .background(color, CircleShape)
    )
}
