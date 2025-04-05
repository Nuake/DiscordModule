#include "DiscordModule.h"
#include "Nuake/Modules/ModuleDB.h"
#include "Nuake/Core/Logger.h"
#include "Engine.h"
#include "Nuake/Resource/Project.h"

#define DISCORDPP_IMPLEMENTATION
#include "discordpp.h"

const uint64_t APPLICATION_ID = 1232753553919967303;

std::shared_ptr<discordpp::Client> client;
std::atomic<bool> isClientReady = false;
std::atomic<bool> statusChanged = false;
std::string richPresenceState;
std::string richPresenceDetails;

void UpdateRichPresence(NativeString state, NativeString details)
{
	if (state != richPresenceState || details != richPresenceDetails)
	{
		statusChanged = true;
		richPresenceState = state;
		richPresenceDetails = details;
	}
}

NUAKEMODULE(DiscordModule)
void DiscordModule_Startup()
{
	using namespace Nuake;

	#pragma comment(lib, "discord_partner_sdk.lib")

	client = CreateRef<discordpp::Client>();
	client->AddLogCallback([](auto message, auto severity) 
	{
		Logger::Log(message, "discord", VERBOSE);
	}, discordpp::LoggingSeverity::Info);


	client->SetStatusChangedCallback([&](discordpp::Client::Status status, discordpp::Client::Error error, int32_t errorDetail) 
	{
		std::cout << "Status changed: " << discordpp::Client::StatusToString(status) << std::endl;

		if (status == discordpp::Client::Status::Ready) 
		{
			isClientReady = true;

			// Configure rich presence details
			discordpp::Activity activity;
			activity.SetType(discordpp::ActivityTypes::Playing);
			activity.SetState(richPresenceState);
			activity.SetDetails(richPresenceDetails);

			// Update rich presence
			client->UpdateRichPresence(activity, [](discordpp::ClientResult result) 
			{
				if (!result.Successful()) 
				{
					Logger::Log("Rich Presence update failed", "discord", CRITICAL);
				}
			});
		}
		else if (error != discordpp::Client::Error::None) 
		{
			Logger::Log("Connection Error: " + discordpp::Client::ErrorToString(error) + "\nDetails:" + std::to_string(errorDetail), "discord", CRITICAL);
		}
	});

	// Generate OAuth2 code verifier for authentication
	auto codeVerifier = client->CreateAuthorizationCodeVerifier();

	// Set up authentication arguments
	discordpp::AuthorizationArgs args{};
	args.SetClientId(APPLICATION_ID);
	args.SetScopes(discordpp::Client::GetDefaultPresenceScopes());
	args.SetCodeChallenge(codeVerifier.Challenge());

	// Begin authentication process
	client->Authorize(args, [&, codeVerifier](auto result, auto code, auto redirectUri) 
	{
		if (!result.Successful()) 
		{
			Logger::Log("Authentification Error: " + result.Error(), "discord", CRITICAL);
			return;
		}
		else 
		{
			Logger::Log("Authorization successful! Getting access token...\n", "discord", VERBOSE);

			// Exchange auth code for access token
			client->GetToken(APPLICATION_ID, code, codeVerifier.Verifier(), redirectUri,
				[&](discordpp::ClientResult result,
					std::string accessToken,
					std::string refreshToken,
					discordpp::AuthorizationTokenType tokenType,
					int32_t expiresIn,
					std::string scope) 
			{
				Logger::Log("Access token received!Establishing connection...", "discord", VERBOSE);

				// Next Step: Update the token and connect
				client->UpdateToken(discordpp::AuthorizationTokenType::Bearer, accessToken, [&](discordpp::ClientResult result) 
				{
					if (result.Successful()) 
					{
						Logger::Log("Token updated, connecting to Discord...", "discord", VERBOSE);
						client->Connect();
					}
				});
			});
		}
	});

	// Register the module & info
	auto& module = ModuleDB::Get().RegisterModule<DiscordModule>();
	module.instance = module.Resolve().construct();
	module.Description = "This a Discord rich presence module";

	// This is to expose parameters in the modules settings
	module.BindFunction<UpdateRichPresence>("UpdateRichPresence", "state", "details");

	module.OnSceneLoad.AddStatic([](Ref<Scene> scene) 
	{
		UpdateRichPresence(NativeString::New(richPresenceState), NativeString::New(scene->Path));
	});

	module.OnGameStateChanged.AddStatic([](GameState state) 
	{
		std::string stateText;
		switch (state)
		{
			case GameState::Stopped:
				stateText = "Editing";
				break;
			case GameState::Playing:
				stateText = "Playing";
				break;
			case GameState::Paused:
				stateText = "Paused";
				break;
			default:
				stateText = "Waiting";
				break;
		}

		UpdateRichPresence(NativeString::New(stateText), NativeString::New(richPresenceDetails));
	});

	// The module can hook to certain events
	module.OnUpdate.AddStatic([](float ts)
	{
	});

	module.OnFixedUpdate.AddStatic([&](float ts) 
	{
		if (statusChanged)
		{
			if (isClientReady)
			{
				discordpp::Activity activity;
				activity.SetType(discordpp::ActivityTypes::Playing);
				activity.SetState(richPresenceState);
				activity.SetDetails(richPresenceDetails);

				client->UpdateRichPresence(activity, [](discordpp::ClientResult result) 
				{
					if (!result.Successful()) 
					{
						Logger::Log("Failed to update Rich Presence", "discord", WARNING);
					}
				});


				statusChanged = false;
			}
		}
		discordpp::RunCallbacks();
	});
}

void DiscordModule_Shutdown()
{

}