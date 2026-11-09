# WiFi scan on the LCD

Open **System → WiFi scan**, then explicitly choose **Start WiFi scan**.
Cancel is selected first. Opening or browsing this screen does not start a scan.
Scanning can interrupt WiFi service; it never selects a connection or saves
settings. The existing network service owns the physical scan and may share its
results with an already waiting browser.

The LCD copies at most 16 results into bounded wiping PSRAM storage. Results show
a stable result number and BSSID to distinguish duplicate names. Choose a result
to review its full SSID and BSSID, then explicitly choose **Use SSID in settings
draft**. Cancel is selected first here too. The LCD uses its existing safe text
fallback for unsupported glyphs; the original SSID bytes are retained in the
draft. Hidden SSIDs and results beyond the limit use **Enter SSID manually**.

Selecting a network opens the existing Network settings form with only its SSID
changed in the draft. Review the WiFi password and other settings, then use that
form's separate Save confirmation. Merely scanning, reviewing or copying the
SSID does not connect, persist a password, change the current network or restart
the controller. Back cancels the draft.

A pending local request expires after 30 seconds. Back, sleep, loss of graphics,
expiry, a changed login/security session or changed network settings cancel the
exact local subscription and erase copied results. A physical shared scan that
was already accepted may finish; cancelling the LCD never cancels or clears a
foreign scan. There is no automatic rescan or retry. Results and review use the
same protected capture and completely presented frame requirement as other
administration screens.

The LCD component retains only a 32-byte lazy handle, borrows the existing
administration presentation frame, and bounds its allocated session plus wiping
allocator header to 1,536 bytes. Its 16 copied rows are each at most 44 bytes.
The scan session is discarded before the separate settings draft is admitted.
These bounds exclude the SDK's heap metadata; no additional frame is retained.

Offline verification covers explicit default Cancel/Start, the 16-result limit,
immutable copies, duplicate identities, hidden/manual entry, a 32-byte wide SSID
including its final glyph, complete warning text, stale/late callbacks, pending
expiry, allocation failures and auth/network revision changes. Production Runtime
encoder journeys check unseen Start/handoff suppression, no save or RF action,
Network draft handoff, cancellation, network changes, sleep and graphics teardown.
The tests use fake SDK/domain endpoints and do not contact a controller or WiFi
network.
