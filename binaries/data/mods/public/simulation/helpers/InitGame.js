function InitGame(settings)
{
	// This will be called after the map settings have been loaded,
	// before the simulation has started.
	// This is only called at the start of a new game, not when loading
	// a saved game.

	// No settings when loading a map in Atlas, so do nothing
	if (!settings)
		return;

	var cmpPlayerManager = Engine.QueryInterface(SYSTEM_ENTITY, IID_PlayerManager);
	var cmpAIManager = Engine.QueryInterface(SYSTEM_ENTITY, IID_AIManager);
	for (var i = 0; i < settings.PlayerData.length; ++i)
	{
		var cmpPlayer = Engine.QueryInterface(cmpPlayerManager.GetPlayerByID(i+1), IID_Player);
		if (!settings.CheatsEnabled)
			cmpPlayer.SetCheatEnabled(false);
		if (settings.PlayerData[i] && settings.PlayerData[i].AI && settings.PlayerData[i].AI != "")
		{
			cmpAIManager.AddPlayer(settings.PlayerData[i].AI, i+1);
			cmpPlayer.SetAI(true);
			cmpPlayer.SetCheatEnabled(true);
		}
		cmpPlayer.maxPop = settings.PopulationCap;

		if (settings.mapType !== "scenario")
			for (var resources in cmpPlayer.resourceCount)
				cmpPlayer.resourceCount[resources] = settings.StartingResources;
	}
	if (settings.TriggerData) {
		var cmpTriggerManager = Engine.QueryInterface(SYSTEM_ENTITY, IID_TriggerManager);
		for (var i = 0; i < settings.TriggerData.length; ++i)
		{
			if (settings.TriggerData[i])
				cmpTriggerManager.AddTrigger(settings.TriggerData[i]);
		}
	}
}

Engine.RegisterGlobal("InitGame", InitGame);
