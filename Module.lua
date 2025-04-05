return {
    name = "Discord Module",
    description = "This a Discord rich presence module",
    module_header = "DiscordModule.h",
    -- Place all your sources here
    sources = {
        "DiscordModule.cpp"
    },
    -- These will get copied in the correct directory for linking
    libs = {
        "discord_partner_sdk.lib",
        "discord_partner_sdk.dll"
    }
}