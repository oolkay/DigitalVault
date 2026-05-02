pragma Singleton
import QtQuick

QtObject {
    id: theme

    property bool isDark: true

    // Toggle
    function toggle() { isDark = !isDark }

    // ── Backgrounds ───────────────────────────────
    property color bgBase:      isDark ? "#0A0C12" : "#F0F2F7"
    property color bgSurface:   isDark ? "#12151F" : "#FFFFFF"
    property color bgCard:      isDark ? "#181C28" : "#F8F9FC"
    property color bgSidebar:   isDark ? "#0E1119" : "#FFFFFF"
    property color bgOverlay:   isDark ? "#1E2335" : "#EDF0F7"
    property color bgInput:     isDark ? "#1A1E2E" : "#EEF0F8"
    property color bgHover:     isDark ? "#252A3D" : "#E6E9F4"

    // ── Borders ───────────────────────────────────
    property color border:      isDark ? "#2A3050" : "#D5D9E8"
    property color borderFocus: isDark ? "#4E6BF5" : "#4E6BF5"

    // ── Text ──────────────────────────────────────
    property color textPrimary:   isDark ? "#E8ECF8" : "#0E1525"
    property color textSecondary: isDark ? "#7A85A8" : "#6B789A"
    property color textMuted:     isDark ? "#404868" : "#9BA5C4"

    // ── Accent ────────────────────────────────────
    property color accent:        "#4E6BF5"
    property color accentHover:   "#6480FF"
    property color accentGlow:    isDark ? "#1A2560" : "#D0D8FF"

    // ── Status ────────────────────────────────────
    property color success:       "#2ECC88"
    property color successBg:     isDark ? "#0D2E20" : "#D4F5E5"
    property color danger:        "#F25565"
    property color dangerBg:      isDark ? "#2E0D14" : "#FADADF"
    property color warning:       "#F5A623"
    property color warningBg:     isDark ? "#2E1F0D" : "#FEF0D5"

    // ── Sidebar nav active ────────────────────────
    property color navActive:     isDark ? "#1E2646" : "#E0E5FF"
    property color navActiveText: "#4E6BF5"

    // ── Shadows / glow radius ─────────────────────
    property int   radius:     16
    property int   radiusSm:    8
}
