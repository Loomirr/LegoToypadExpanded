// TTGames Workshop extension for the pinned MIT LegoToypad source.
// Included inside main.cpp's namespace, after BuildCustomTagList.
// No executable plugins: manifests select names and sibling images/tags only.
void ApplyCustomOrderFromIni();
void LoadFranchiseOrderFromIni();
void RebuildFranchiseDisplay();
extern std::map<int, Gdiplus::Rect> g_opaqueBoundsCache;

constexpr size_t kWorkshopMaxPacks = 64;
constexpr size_t kWorkshopMaxFiles = 512;
constexpr size_t kWorkshopMaxTags = 2048;
size_t g_workshopTagCount = 0;
size_t g_workshopStockCount = 0;
std::vector<size_t> g_workshopBranded;
std::wstring g_workshopFingerprint;
ULONGLONG g_workshopNextPoll = 0;
unsigned long long g_workshopRevision = 0;
std::mutex g_workshopFilesMutex;

bool WorkshopPlainPath(const std::filesystem::path& path, bool directory)
{
    const DWORD flags = GetFileAttributesW(path.c_str());
    return flags != INVALID_FILE_ATTRIBUTES && !(flags & FILE_ATTRIBUTE_REPARSE_POINT) &&
        ((flags & FILE_ATTRIBUTE_DIRECTORY) != 0) == directory;
}

std::vector<std::filesystem::path> WorkshopChildren(const std::filesystem::path& root, bool folders, size_t limit)
{
    std::vector<std::filesystem::path> paths;
    if (!WorkshopPlainPath(root, true)) return paths;
    std::error_code ec;
    std::filesystem::directory_iterator it(root, ec), end;
    size_t scanned = 0;
    for (; !ec && it != end && scanned < limit; it.increment(ec), ++scanned)
        if (WorkshopPlainPath(it->path(), folders)) paths.push_back(it->path());
    std::sort(paths.begin(), paths.end(), [](const auto& a, const auto& b) {
        return _wcsicmp(a.c_str(), b.c_str()) < 0;
    });
    return paths;
}

bool WorkshopSmallFile(const std::filesystem::path& path, uintmax_t limit)
{
    std::error_code ec;
    return WorkshopPlainPath(path, false) && std::filesystem::file_size(path, ec) <= limit && !ec;
}

bool WorkshopTitle(const std::wstring& text)
{
    if (text.empty() || text.size() > 64 || text.front() == L' ' || text.back() == L' ') return false;
    for (wchar_t ch : text) if (ch < 32 || (ch >= 127 && ch <= 159)) return false;
    return true;
}

std::wstring WorkshopIni(const std::filesystem::path& path, const wchar_t* section, const wchar_t* key)
{
    if (!WorkshopSmallFile(path, 16384)) return {};
    std::ifstream input(path, std::ios::binary);
    const std::string bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    if (bytes.empty() || bytes.size() > 16384) return {};
    std::wstring text;
    const auto b = [&](size_t n) { return static_cast<unsigned char>(bytes[n]); };
    if (bytes.size() >= 2 && ((b(0) == 255 && b(1) == 254) || (b(0) == 254 && b(1) == 255)))
    {
        if (bytes.size() % 2) return {};
        for (size_t i = 2; i < bytes.size(); i += 2)
            text.push_back(static_cast<wchar_t>(b(0) == 255 ? b(i) | (b(i + 1) << 8) : (b(i) << 8) | b(i + 1)));
        if (!text.empty() && !WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(),
            static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr)) return {};
    }
    else
    {
        const size_t start = bytes.size() >= 3 && b(0) == 239 && b(1) == 187 && b(2) == 191 ? 3 : 0;
        const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data() + start,
                                             static_cast<int>(bytes.size() - start), nullptr, 0);
        if (count <= 0) return {};
        text.resize(count);
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data() + start,
                            static_cast<int>(bytes.size() - start), text.data(), count);
    }
    if (text.find(L'\0') != std::wstring::npos) return {};
    const auto trim = [](std::wstring value) {
        const auto first = value.find_first_not_of(L" \t\r");
        return first == std::wstring::npos ? std::wstring() :
            value.substr(first, value.find_last_not_of(L" \t\r") - first + 1);
    };
    std::wistringstream lines(text);
    std::wstring line, result;
    bool active = false, found = false;
    while (std::getline(lines, line))
    {
        line = trim(line);
        if (line.empty() || line[0] == L';' || line[0] == L'#') continue;
        if (line.front() == L'[' && line.back() == L']')
        {
            active = _wcsicmp(trim(line.substr(1, line.size() - 2)).c_str(), section) == 0;
            continue;
        }
        const auto equal = line.find(L'=');
        if (!active || equal == std::wstring::npos || _wcsicmp(trim(line.substr(0, equal)).c_str(), key)) continue;
        if (found) return {}; // Ambiguous duplicate keys are invalid.
        result = trim(line.substr(equal + 1));
        if (result.size() > 64) return {};
        found = true;
    }
    return result;
}

int WorkshopPathId(std::vector<std::filesystem::path>& paths, const std::filesystem::path& path)
{
    // Never reuse a removed file's id during this process. Loaded pads and web
    // clients can retain negative ids while a pack is installed or removed.
    for (size_t i = 0; i < paths.size(); ++i)
        if (_wcsicmp(paths[i].c_str(), path.c_str()) == 0) return -static_cast<int>(i + 1);
    if (paths.size() >= 16384) return 0;
    paths.push_back(path);
    return -static_cast<int>(paths.size());
}

int WorkshopImage(const std::filesystem::path& path)
{
    if (!WorkshopSmallFile(path, 8 * 1024 * 1024)) return 0;
    Gdiplus::Bitmap image(path.c_str());
    if (image.GetLastStatus() != Gdiplus::Ok || image.GetWidth() > 2048 || image.GetHeight() > 2048 ||
        !image.GetWidth() || !image.GetHeight()) return 0;
    return WorkshopPathId(g_customPortraitPaths, path);
}

int WorkshopPortrait(const std::filesystem::path& bin)
{
    for (const wchar_t* extension : {L".png", L".jpg", L".jpeg"})
    {
        auto path = bin;
        path.replace_extension(extension);
        const int id = WorkshopImage(path);
        if (id != 0) return id;
    }
    return 0;
}

bool WorkshopEntry(const std::filesystem::path& bin, const std::wstring& name, RosterEntry& entry)
{
    std::error_code ec;
    if (g_workshopTagCount >= kWorkshopMaxTags || !WorkshopTitle(name) ||
        !WorkshopPlainPath(bin, false) || std::filesystem::file_size(bin, ec) != kTagSize || ec) return false;
    entry.binResourceId = WorkshopPathId(g_customBinPaths, bin);
    if (entry.binResourceId == 0) return false;
    entry.portraitResourceId = WorkshopPortrait(bin);
    entry.name = name;
    entry.stableId = L"workshop:" + bin.lexically_relative(GetExecutableDirectory()).generic_wstring();
    entry.ringColor = RingColorForName(name);
    entry.buildNumber = 0;
    ++g_workshopTagCount;
    return true;
}

void WorkshopBrand(const std::wstring& category, int logo)
{
    if (!logo) return;
    for (size_t i = g_workshopStockCount; i < kFranchises.size(); ++i)
        if (_wcsicmp(kFranchises[i].name.c_str(), category.c_str()) == 0)
        {
            // Explicit artwork replaces only the generated portrait fallback.
            // Stock tiles and the first explicit logo remain authoritative.
            if (std::find(g_workshopBranded.begin(), g_workshopBranded.end(), i) == g_workshopBranded.end())
            {
                kFranchises[i].logoResourceId = logo;
                g_workshopBranded.push_back(i);
            }
            return;
        }
}

void WorkshopCategorize(RosterEntry entry, const std::wstring& category, int logo = 0)
{
    auto franchise = std::find_if(kFranchises.begin(), kFranchises.end(), [&](const Franchise& item) {
        return _wcsicmp(item.name.c_str(), category.c_str()) == 0;
    });
    if (franchise == kFranchises.end())
    {
        Franchise item;
        item.name = category;
        item.logoResourceId = entry.portraitResourceId != 0 ? entry.portraitResourceId : kCustomTileResourceId;
        kFranchises.push_back(std::move(item));
        franchise = std::prev(kFranchises.end());
    }
    WorkshopBrand(category, logo);
    // A second pack may extend a category, but cannot replace an existing entry.
    for (const auto& existing : franchise->characters)
        if (_wcsicmp(existing.name.c_str(), entry.name.c_str()) == 0) return;
    franchise->characters.push_back(std::move(entry));
}

std::wstring WorkshopFingerprint()
{
    std::wstring stamp;
    for (const wchar_t* rootName : {L"Special", L"Categories"})
    {
        const auto root = GetExecutableDirectory() / rootName;
        for (const auto& folder : WorkshopChildren(root, true, kWorkshopMaxPacks))
        {
            stamp += folder.wstring() + L"\n";
            for (const auto& file : WorkshopChildren(folder, false, kWorkshopMaxFiles))
            {
                std::error_code ec;
                const auto size = std::filesystem::file_size(file, ec);
                if (ec) continue;
                const auto modified = std::filesystem::last_write_time(file, ec);
                if (ec) continue;
                stamp += file.filename().wstring() + L":" + std::to_wstring(size) + L":" +
                    std::to_wstring(modified.time_since_epoch().count()) + L"\n";
            }
        }
    }
    return stamp;
}

void BuildWorkshopCategories()
{
    const std::lock_guard<std::mutex> filesLock(g_workshopFilesMutex);
    static const std::vector<Franchise> stock = kFranchises;
    kFranchises = stock;
    g_workshopStockCount = stock.size();
    g_workshopBranded.clear();
    g_customEntries.clear();
    g_workshopTagCount = 0;
    ReleaseCustomPortraitBitmaps();
    ReleaseGlossCache();
    g_opaqueBoundsCache.clear();
    const auto root = GetExecutableDirectory();
    for (const auto& folder : WorkshopChildren(root / L"Special", true, kWorkshopMaxPacks))
    {
        for (const auto& bin : WorkshopChildren(folder, false, kWorkshopMaxFiles))
        {
            if (_wcsicmp(bin.extension().c_str(), L".bin") != 0) continue;
            RosterEntry entry;
            if (!WorkshopEntry(bin, bin.stem().wstring(), entry)) continue;
            const auto metadata = folder / L"creator.ini";
            const auto category = WorkshopIni(metadata, L"Creator", L"Category");
            const auto name = WorkshopIni(metadata, L"Creator", L"Name");
            const auto uuid = WorkshopIni(metadata, L"Creator", L"Uuid");
            const auto identity = folder.filename().wstring();
            bool validUuid = uuid.size() == 32 && identity == L"cc_" + uuid;
            for (wchar_t ch : uuid) validUuid = validUuid &&
                ((ch >= L'0' && ch <= L'9') || (ch >= L'a' && ch <= L'f'));
            if (validUuid && WorkshopIni(metadata, L"Creator", L"Schema") == L"1" &&
                WorkshopTitle(category) && WorkshopTitle(name))
            {
                entry.name = name;
                entry.stableId = identity;
                const int portrait = WorkshopImage(folder / L"portrait.png");
                if (portrait != 0) entry.portraitResourceId = portrait;
                WorkshopCategorize(std::move(entry), category, WorkshopImage(folder / L"category.png"));
            }
            else g_customEntries.push_back(std::move(entry));
            break;
        }
    }
    const auto packs = WorkshopChildren(root / L"Categories", true, kWorkshopMaxPacks);
    for (const auto& folder : packs)
    {
        const auto metadata = folder / L"category.ini";
        const auto name = WorkshopIni(metadata, L"Category", L"Name");
        if (WorkshopIni(metadata, L"Category", L"Schema") != L"1" || !WorkshopTitle(name)) continue;
        const int logo = WorkshopImage(folder / L"logo.png");
        for (const auto& bin : WorkshopChildren(folder, false, kWorkshopMaxFiles))
        {
            if (_wcsicmp(bin.extension().c_str(), L".bin") != 0) continue;
            RosterEntry entry;
            if (WorkshopEntry(bin, bin.stem().wstring(), entry)) WorkshopCategorize(std::move(entry), name, logo);
        }
    }
    // Metadata-only packs can supply art without duplicate tags, including
    // categories first introduced by a later folder in the scan.
    for (const auto& folder : packs)
    {
        const auto metadata = folder / L"category.ini";
        const auto name = WorkshopIni(metadata, L"Category", L"Name");
        if (WorkshopIni(metadata, L"Category", L"Schema") == L"1" && WorkshopTitle(name))
            WorkshopBrand(name, WorkshopImage(folder / L"logo.png"));
    }
    kFranchiseCount = kFranchises.size();
    g_catalogBuilt = false;
    g_catalogCache.clear();
    ++g_workshopRevision;
    g_workshopFingerprint = WorkshopFingerprint();
}

void WorkshopRefresh(HWND window)
{
    g_app.rosterSlots.clear();
    g_app.plusGroup = nullptr;
    g_app.rosterIndex = g_app.rosterTopRow = g_app.plusBuildIndex = 0;
    g_app.franchiseIndex = g_app.franchiseTopRow = 0;
    g_app.reorganizingRoster = g_app.reorganizingFranchise = false;
    g_app.storyRosterActive = g_app.abilityRosterActive = false;
    g_app.virtualTile = VirtualTile::None;
    if (g_app.screen == Screen::RosterList || g_app.screen == Screen::PlusPicker)
        g_app.screen = Screen::FranchiseList;
    // Pad slots are value objects. No LOAD/REMOVE command is sent to the game.
    BuildWorkshopCategories();
    ApplyCustomOrderFromIni();
    LoadFranchiseOrderFromIni();
    RebuildFranchiseDisplay();
    g_app.status = L"Categories refreshed (F5)";
    InvalidateRect(window, nullptr, FALSE);
}

void WorkshopPoll(HWND window)
{
    const ULONGLONG now = GetTickCount64();
    if (now < g_workshopNextPoll) return;
    g_workshopNextPoll = now + 2000;
    if (g_app.capturingKeyIndex >= 0 || g_app.capturingBindingIndex >= 0 || g_app.capturingShortcut ||
        g_app.draggingWindow || g_app.reorganizingFranchise || g_app.reorganizingRoster ||
        (g_app.overlayVisible && g_app.screen != Screen::PadViewer)) return;
    if (WorkshopFingerprint() != g_workshopFingerprint) WorkshopRefresh(window);
}
