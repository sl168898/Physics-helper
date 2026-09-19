Scriptname AGH_SR_MCM extends SKI_ConfigBase
; Preserve the original quest and script identity on existing saves.
; Settings now live in SKSE Menu Framework. Never register a new MCM page.
Event OnInit()
    OnGameReload()
EndEvent
Event OnGameReload()
    UnregisterForUpdate()
    RegisterForModEvent("SKICP_configManagerReady", "OnConfigManagerReady")
EndEvent
Event OnConfigManagerReady(string eventName, string strArg, float numArg, Form sender)
    SKI_ConfigManager manager = sender as SKI_ConfigManager
    if manager
        int removed = manager.UnregisterMod(self)
        ; Keep listening if SkyUI was busy; its next ready event retries safely.
        if removed >= 0
            UnregisterForModEvent("SKICP_configManagerReady")
        endif
    endif
EndEvent
Event OnConfigManagerReset(string eventName, string strArg, float numArg, Form sender)
EndEvent
Event OnConfigInit()
EndEvent
Event OnUpdate()
EndEvent
Int Function GetVersion()
    Return 12000
EndFunction
