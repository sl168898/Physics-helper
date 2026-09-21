Scriptname WPD_Storage Hidden

Bool Function IsReady() Global Native
Potion Function ProtectNative(Potion liquid, String chosenName) Global Native
Function TraceNative(String message) Global Native

Function Trace(String message) Global
    ; Optional diagnostics. Older DLLs must never receive an unbound call.
    If SKSE.GetPluginVersion("WhitePhialNames") >= 33554448
        TraceNative(message)
    EndIf
EndFunction

Potion Function Protect(Potion liquid) Global
    If !liquid || SKSE.GetPluginVersion("WhitePhialNames") < 33554432
        Return None
    EndIf
    If !IsReady()
        Return None
    EndIf
    Return ProtectNative(liquid, WPD_Names.GetBottleName(liquid))
EndFunction
