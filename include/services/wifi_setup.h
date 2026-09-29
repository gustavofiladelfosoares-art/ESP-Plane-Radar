#pragma once

/** True when the next boot should show the setup screen first (after credential reset). */
bool wifiShowsSetupScreenOnBoot();
void wifiResetCredentialsAndReboot();
/** Boot flow: connect with UI, open portal only if saved creds fail. */
bool wifiSetupConnect();
/** Reconnect using saved creds; never opens the captive portal. */
bool wifiReconnect();
/** Keeps the LAN config portal alive; call every loop() iteration. */
void wifiLoop();
bool wifiBootButtonPressed();
/** GPIO + interrupt setup; call once early in setup(). */
void bootButtonInit();
enum class BootGesture { None, Tap, DoubleTap };
/**
 * Latched BOOT gesture (survives blocking work). A single tap is reported
 * once the double-tap window has passed without a second tap.
 */
BootGesture bootButtonConsumeGesture();
/** Call each loop iteration; triggers WiFi reset on long hold. */
void bootButtonPollLongPress();
