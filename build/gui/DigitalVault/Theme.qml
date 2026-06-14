pragma Singleton
import QtQuick

QtObject {
    id: theme

    property bool isDark: true

    // Toggle
    function toggle() { isDark = !isDark }

    // ── Backgrounds ───────────────────────────────
    property color bgBase:      isDark ? "#1E1E1E" : "#F4F6F8"
    property color bgSurface:   isDark ? "#252526" : "#FFFFFF"
    property color bgCard:      isDark ? "#252526" : "#F9FAFB"
    property color bgSidebar:   isDark ? "#252526" : "#FFFFFF"
    property color bgOverlay:   isDark ? "#2D2D30" : "#EEF2F5"
    property color bgInput:     isDark ? "#3C3C3C" : "#F0F3F6"
    property color bgHover:     isDark ? "#2A2D2E" : "#E8EDF2"

    // ── Borders ───────────────────────────────────
    property color border:      isDark ? "#3C3C3C" : "#D8DEE6"
    property color borderFocus: isDark ? "#007ACC" : "#247BA0"

    // ── Text ──────────────────────────────────────
    property color textPrimary:   isDark ? "#D4D4D4" : "#111827"
    property color textSecondary: isDark ? "#CCCCCC" : "#5E6B78"
    property color textMuted:     isDark ? "#858585" : "#8793A0"

    // ── Accent ────────────────────────────────────
    property color accent:        isDark ? "#007ACC" : "#247BA0"
    property color accentHover:   isDark ? "#1177BB" : "#1C6F91"
    property color accentGlow:    isDark ? "#094771" : "#D7EEF6"

    // ── Status ────────────────────────────────────
    property color success:       isDark ? "#89D185" : "#3FB984"
    property color successBg:     isDark ? "#1E3A2B" : "#DDF3EA"
    property color danger:        isDark ? "#F48771" : "#E05A5A"
    property color dangerBg:      isDark ? "#4B1F1A" : "#F7DFDF"
    property color warning:       isDark ? "#CCA700" : "#D99A2B"
    property color warningBg:     isDark ? "#3A3200" : "#F7EBD2"

    // ── Sidebar nav active ────────────────────────
    property color navActive:     isDark ? "#094771" : "#DDEFF5"
    property color navActiveText: accent

    // ── Shadows / glow radius ─────────────────────
    property int   radius:     10
    property int   radiusSm:    8

    // ── Typography ─────────────────────────────────
    property string fontFamily: "Noto Sans"
    property string monoFamily: "Courier New"
}
