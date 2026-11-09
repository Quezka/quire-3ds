// Read-only sync with a quire-server (docs: quire-server/docs/PROTOCOL.md): refresh the access token, then pull
// every record changed since the saved cursor. Nothing is ever sent back.
#pragma once
#include "link.h"
#include "store.h"

typedef void (*SyncProgress)(int records, void *context);

// Returns NULL when it worked, else a message for the person. `ca_file` is the bundle of trusted certificates.
const char *sync_pull(const QuireLink *link, Store *store, const char *ca_file, SyncProgress progress, void *context);
