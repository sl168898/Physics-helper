Scriptname WPD_Blacklist Hidden

Bool Function IsBlockedNative(Potion liquid) Global Native

Bool Function IsBlocked(Potion liquid) Global
    ; Refuse duplication if the matching native update is missing.
    If !liquid || SKSE.GetPluginVersion("WhitePhialNames") < 33619968
        Return True
    EndIf
    Return IsBlockedNative(liquid)
EndFunction
