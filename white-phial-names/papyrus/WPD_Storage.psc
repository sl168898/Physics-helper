Scriptname WPD_Storage Hidden

Bool Function IsReady() Global Native
Potion Function ProtectNative(Potion liquid, String chosenName) Global Native

Potion Function Protect(Potion liquid) Global
    If !liquid || SKSE.GetPluginVersion("WhitePhialNames") < 33554432
        Return None
    EndIf
    If !IsReady()
        Return None
    EndIf
    Return ProtectNative(liquid, WPD_Names.GetBottleName(liquid))
EndFunction
