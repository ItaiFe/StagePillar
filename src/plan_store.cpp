#include "plan_store.h"
#include <HTTPClient.h>
#include <LittleFS.h>
#include "config.h"

namespace {

// Reads a plan file on demand; each read opens, seeks and closes it.
class FileSource : public ByteSource {
public:
    void setPath(const String& path) { path_ = path; }
    uint32_t size() const override {
        File f = LittleFS.open(path_, "r");
        uint32_t n = f ? f.size() : 0;
        f.close();
        return n;
    }
    bool read(uint32_t offset, uint8_t* dst, uint32_t len) const override {
        File f = LittleFS.open(path_, "r");
        bool ok = f && f.seek(offset) && f.read(dst, len) == len;
        f.close();
        return ok;
    }

private:
    String path_;
};

const char* kDir = "/plans";
const char* kStaging = "/plans/new";
const char* kVersionFile = "/plans/version";
const char* kPreviewStaging = "/preview.new";
const char* kPreviewFile = "/preview.bin";

// loop() only.
FileSource sources[kStoredSlots];
PlanIndex indexes[kStoredSlots];
bool valid[kStoredSlots];
FileSource previewSource;
PlanIndex previewIndex;

// Handed between the tasks.
volatile uint32_t activeVersion = 0;
volatile uint32_t stagedVersion = 0;   // non-zero: a complete round waits in kStaging
volatile uint32_t stagedPreviewId = 0; // non-zero: a preview waits in kPreviewStaging
volatile bool previewEnded = false;

// Show-state task only.
uint32_t lastPreviewId = 0;
uint32_t failedVersion = 0;
uint32_t failedAtMs = 0;

String slotPath(const char* dir, Slot slot) { return String(dir) + "/" + slotName(slot) + ".bin"; }
String nonePath(Slot slot) { return String(kStaging) + "/" + slotName(slot) + ".none"; }

int slotIndex(Slot slot) {
    for (int i = 0; i < kStoredSlots; i++) {
        if (kSlots[i] == slot) return i;
    }
    return -1;
}

void loadSlots() {
    for (int i = 0; i < kStoredSlots; i++) {
        String path = slotPath(kDir, kSlots[i]);
        sources[i].setPath(path);
        valid[i] = LittleFS.exists(path) && planIndex(sources[i], indexes[i]);
        if (LittleFS.exists(path) && !valid[i]) Serial.printf("Plans: %s is invalid, using default\n", path.c_str());
    }
}

uint32_t readVersionFile() {
    File f = LittleFS.open(kVersionFile, "r");
    if (!f) return 0;
    uint32_t v = f.readString().toInt();
    f.close();
    return v;
}

// Downloads url to path. 1 = saved, 0 = 404 (not set), -1 = failed.
int download(const String& url, const char* path) {
    HTTPClient http;
    http.setConnectTimeout(HTTP_TIMEOUT_MS);
    http.setTimeout(HTTP_TIMEOUT_MS);
    if (!http.begin(url)) return -1;
    int code = http.GET();
    int result = -1;
    if (code == 404) {
        result = 0;
    } else if (code == 200 && http.getSize() > 0 && uint32_t(http.getSize()) <= kPlanMaxBytes) {
        File f = LittleFS.open(path, "w");
        if (f) {
            int written = http.writeToStream(&f);
            f.close();
            if (written == http.getSize()) result = 1;
        }
    }
    http.end();
    if (result < 0) Serial.printf("Plans: GET %s -> %d\n", url.c_str(), code);
    return result;
}

// True when path holds a valid plan whose header carries `version`.
bool validFile(const char* path, uint32_t version) {
    FileSource src;
    src.setPath(path);
    PlanIndex index;
    return planIndex(src, index) && index.version == version;
}

void clearStaging() {
    File dir = LittleFS.open(kStaging);
    if (!dir) return;
    for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
        String path = String(kStaging) + "/" + f.name();
        f.close();
        LittleFS.remove(path);
    }
    dir.close();
}

// Fetches all six slots for `version` into the staging folder.
bool downloadRound(IPAddress server, uint32_t version) {
    clearStaging();
    String base = "http://" + server.toString() + ":" + String(SERVER_PORT) + "/api/pillar/plans/";
    for (int i = 0; i < kStoredSlots; i++) {
        Slot slot = kSlots[i];
        String path = slotPath(kStaging, slot);
        int r = download(base + slotName(slot) + ".bin", path.c_str());
        if (r < 0) return false;
        if (r == 0) {
            File f = LittleFS.open(nonePath(slot), "w");
            f.close();
            continue;
        }
        // A save during the round changes the version; give up and retry with the new one.
        if (!validFile(path.c_str(), version)) {
            Serial.printf("Plans: %s is not a valid v%u file\n", slotName(slot), (unsigned)version);
            return false;
        }
    }
    return true;
}

}  // namespace

void planStoreBegin() {
    if (!LittleFS.begin(true)) {
        Serial.println("Plans: LittleFS mount failed, using defaults");
        return;
    }
    LittleFS.mkdir(kDir);
    LittleFS.mkdir(kStaging);
    loadSlots();
    activeVersion = readVersionFile();
    Serial.printf("Plans: version %u\n", (unsigned)activeVersion);
}

Plan planStoreLookup(Slot slot) {
    int i = slotIndex(slot);
    if (i < 0 || !valid[i]) return defaultPlan(slot);
    return Plan{&sources[i], &indexes[i]};
}

bool planStoreApplyPending() {
    uint32_t version = stagedVersion;
    if (version == 0) return false;
    for (int i = 0; i < kStoredSlots; i++) {
        Slot slot = kSlots[i];
        String active = slotPath(kDir, slot);
        String staged = slotPath(kStaging, slot);
        LittleFS.remove(active);
        if (LittleFS.exists(staged)) LittleFS.rename(staged, active);
        LittleFS.remove(nonePath(slot));
    }
    File f = LittleFS.open(kVersionFile, "w");
    f.print(version);
    f.close();
    loadSlots();
    activeVersion = version;
    stagedVersion = 0;
    Serial.printf("Plans: now on version %u\n", (unsigned)version);
    return true;
}

bool planStoreTakePreview(Plan& out) {
    if (stagedPreviewId == 0) return false;
    LittleFS.remove(kPreviewFile);
    LittleFS.rename(kPreviewStaging, kPreviewFile);
    stagedPreviewId = 0;
    previewSource.setPath(kPreviewFile);
    if (!planIndex(previewSource, previewIndex)) return false;
    out = Plan{&previewSource, &previewIndex};
    return true;
}

bool planStoreTakePreviewEnd() {
    if (!previewEnded) return false;
    previewEnded = false;
    return true;
}

uint32_t planStoreVersion() {
    return activeVersion;
}

void planStoreSync(IPAddress server, uint32_t version, uint32_t previewId) {
    if (previewId != lastPreviewId && stagedPreviewId == 0) {
        if (previewId == 0) {
            previewEnded = true;
            lastPreviewId = 0;
        } else {
            // Retried on the next poll if it fails; preview ids expire on the server after 60 s.
            String url = "http://" + server.toString() + ":" + String(SERVER_PORT) + "/api/pillar/preview.bin";
            if (download(url, kPreviewStaging) == 1 && validFile(kPreviewStaging, previewId)) {
                stagedPreviewId = previewId;
                lastPreviewId = previewId;
            }
        }
    }

    if (version == activeVersion || stagedVersion != 0) return;
    // After a failed round, wait 5 s before retrying the same version.
    if (version == failedVersion && millis() - failedAtMs < 5000) return;
    Serial.printf("Plans: fetching version %u\n", (unsigned)version);
    if (downloadRound(server, version)) {
        stagedVersion = version;
    } else {
        failedVersion = version;
        failedAtMs = millis();
    }
}
