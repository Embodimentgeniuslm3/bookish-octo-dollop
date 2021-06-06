// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ApplicationState.h"
#include "CascadiaSettings.h"
#include "ApplicationState.g.cpp"

#include "JsonUtils.h"
#include "FileUtils.h"

static constexpr std::string_view CloseAllTabsWarningDismissedKey{ "closeAllTabsWarningDismissed" };
static constexpr std::string_view LargePasteWarningDismissedKey{ "largePasteWarningDismissed" };
static constexpr std::string_view MultiLinePasteWarningDismissedKey{ "multiLinePasteWarningDismissed" };

using namespace ::Microsoft::Terminal::Settings::Model;

// Function Description:
// - Returns the path to the state file that lives next to the user's settings.
static std::filesystem::path _statePath()
{
    const auto statePath{ GetBaseSettingsPath() / L"state.json" };
    return statePath;
}

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    // Function Description:
    // - Returns a mutex and storage location for the application-global ApplicationState object
    static std::tuple<std::reference_wrapper<std::mutex>, std::reference_wrapper<winrt::com_ptr<ApplicationState>>> _getStaticStorage()
    {
        static std::mutex mutex{};
        static winrt::com_ptr<ApplicationState> storage{ nullptr };
        return std::make_tuple(std::ref(mutex), std::ref(storage));
    }

    // Method Description:
    // - Returns the application-global ApplicationState object.
    Microsoft::Terminal::Settings::Model::ApplicationState ApplicationState::GetForCurrentApp()
    {
        auto [mtx, state] = _getStaticStorage();
        std::lock_guard<std::mutex> lock{ mtx };
        auto& s{ state.get() };
        if (s && !s->_invalidated)
        {
            return *s;
        }

        auto newState{ winrt::make_self<ApplicationState>(_statePath()) };
        newState->_load();
        s = newState;
        return *newState;
    }

    // Method Description:
    // - Deserializes a state.json document into an ApplicationState.
    // - *ANY* errors will result in the creation of a new empty state
    void ApplicationState::_load()
    {
        wil::unique_hfile hFile{ CreateFileW(_path.c_str(),
                                             GENERIC_READ,
                                             FILE_SHARE_READ | FILE_SHARE_WRITE,
                                             nullptr,
                                             OPEN_EXISTING,
                                             FILE_ATTRIBUTE_NORMAL,
                                             nullptr) };

        if (hFile)
        {
            try
            {
                const auto data{ ReadUTF8TextFileFull(hFile.get()) };

                std::string errs; // This string will receive any error text from failing to parse.
                std::unique_ptr<Json::CharReader> reader{ Json::CharReaderBuilder::CharReaderBuilder().newCharReader() };

                // Parse the json data into either our defaults or user settings. We'll keep
                // these original json values around for later, in case we need to parse
                // their raw contents again.
                Json::Value root;
                // `parse` will return false if it fails.
                if (reader->parse(data.data(), data.data() + data.size(), &root, &errs))
                {
                    LayerJson(root);
                }
            }
            CATCH_LOG();
        }
    }

    // Method Description:
    // - Deserializes a JSON document into the current ApplicationState
    void ApplicationState::LayerJson(const Json::Value& document)
    {
        JsonUtils::GetValueForKey(document, CloseAllTabsWarningDismissedKey, _CloseAllTabsWarningDismissed);
        JsonUtils::GetValueForKey(document, LargePasteWarningDismissedKey, _LargePasteWarningDismissed);
        JsonUtils::GetValueForKey(document, MultiLinePasteWarningDismissedKey, _MultiLinePasteWarningDismissed);
    }

    // Method Description:
    // - Creates a JSON document from the current ApplicationState
    Json::Value ApplicationState::ToJson() const
    {
        Json::Value document{ Json::objectValue };
        JsonUtils::SetValueForKey(document, CloseAllTabsWarningDismissedKey, _CloseAllTabsWarningDismissed);
        JsonUtils::SetValueForKey(document, LargePasteWarningDismissedKey, _LargePasteWarningDismissed);
        JsonUtils::SetValueForKey(document, MultiLinePasteWarningDismissedKey, _MultiLinePasteWarningDismissed);
        return document;
    }

    // Method Description:
    // - Unhooks the current application state from global storage so that a subsequent request will
    //   reload it (public)
    void ApplicationState::Reload()
    {
        _invalidated = true;
    }

    // Method Description:
    // - Deletes the application global state, deleting it from disk and unregistering it globally.
    //   On the next call to GetForCurrentApp, a new state will be created.
    void ApplicationState::Reset()
    {
        _delete();
        _invalidated = true;
    }

    // Method Description:
    // - Writes this application state to disk as JSON, overwriting whatever was there originally.
    void ApplicationState::Commit()
    {
        if (_invalidated)
        {
            // We were destroyed, don't write.
            return;
        }

        Json::StreamWriterBuilder wbuilder;
        const auto content{ Json::writeString(wbuilder, ToJson()) };
        wil::unique_hfile hOut{ CreateFileW(_path.c_str(),
                                            GENERIC_WRITE,
                                            FILE_SHARE_READ | FILE_SHARE_WRITE,
                                            nullptr,
                                            CREATE_ALWAYS,
                                            FILE_ATTRIBUTE_NORMAL,
                                            nullptr) };
        THROW_LAST_ERROR_IF(!hOut);
        THROW_LAST_ERROR_IF(!WriteFile(hOut.get(), content.data(), gsl::narrow<DWORD>(content.size()), nullptr, nullptr));
    }

    // Method Description:
    // - Deletes this instance of state from disk.
    void ApplicationState::_delete()
    {
        std::filesystem::remove(_path);
        _invalidated = true; // don't write the file later -- just in case somebody has an outstanding reference
    }
}
