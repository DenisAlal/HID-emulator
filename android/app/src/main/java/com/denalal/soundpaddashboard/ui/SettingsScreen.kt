package com.denalal.soundpaddashboard.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.text.input.VisualTransformation
import androidx.compose.ui.unit.dp

@Composable
fun SettingsScreen(
    dark: Boolean,
    onDarkChange: (Boolean) -> Unit,
    host: String,
    port: String,
    token: String,
    onHostChange: (String) -> Unit,
    onPortChange: (String) -> Unit,
    onTokenChange: (String) -> Unit,
    onSave: () -> Unit,
    onReload: () -> Unit
) {
    var portInvalid by remember { mutableStateOf(false) }
    var tokenVisible by remember { mutableStateOf(false) }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp)
    ) {
        Text("Connection", style = MaterialTheme.typography.titleMedium)

        OutlinedTextField(
            value = host,
            onValueChange = onHostChange,
            label = { Text("Server host") },
            placeholder = { Text("192.168.1.50") },
            singleLine = true,
            modifier = Modifier.fillMaxWidth()
        )

        OutlinedTextField(
            value = port,
            onValueChange = { value ->
                onPortChange(value.filter { it.isDigit() }.take(5))
                portInvalid = false
            },
            label = { Text("Port") },
            singleLine = true,
            isError = portInvalid,
            supportingText = if (portInvalid) {
                { Text("1-65535") }
            } else {
                null
            },
            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
            modifier = Modifier.fillMaxWidth()
        )

        OutlinedTextField(
            value = token,
            onValueChange = onTokenChange,
            label = { Text("Secret token") },
            placeholder = { Text("secret") },
            singleLine = true,
            visualTransformation = if (tokenVisible) VisualTransformation.None else PasswordVisualTransformation(),
            trailingIcon = {
                TextButton(onClick = { tokenVisible = !tokenVisible }) {
                    Text(if (tokenVisible) "Hide" else "Show")
                }
            },
            modifier = Modifier.fillMaxWidth()
        )

        Spacer(Modifier.height(4.dp))
        Text("Appearance", style = MaterialTheme.typography.titleMedium)

        Row(
            modifier = Modifier.fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.SpaceBetween
        ) {
            Column {
                Text("Dark theme", style = MaterialTheme.typography.bodyLarge)
                Text(
                    "Switch between dark and light",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
            }
            Switch(checked = dark, onCheckedChange = onDarkChange)
        }

        Spacer(Modifier.height(4.dp))

        Button(
            onClick = {
                val p = port.toIntOrNull()
                portInvalid = p == null || p !in 1..65535
                if (!portInvalid && host.isNotBlank()) onSave()
            },
            enabled = host.isNotBlank(),
            modifier = Modifier.fillMaxWidth()
        ) {
            Text("Save")
        }

        TextButton(
            onClick = onReload,
            enabled = host.isNotBlank(),
            modifier = Modifier.fillMaxWidth()
        ) {
            Text("Reload data")
        }
    }
}
