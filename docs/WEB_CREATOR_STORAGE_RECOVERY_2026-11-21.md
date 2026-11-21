# Creator storage resilience: no false saves or lost edits

The browser's save helper previously cleared the dirty flag and swallowed localStorage failures, so an interrupted or quota-exhausted write could incorrectly be reported as a success. It now returns a truthful boolean and preserves the dirty flag for future retries. The editor and import dialogs use that result:

- **New editor level:** if a write fails, undo the pending in-memory append, restore the existing ID and creation counter, and show a copyable `MX1.` backup code. The player remains in the editor and can retry.
- **Existing custom level:** restore the exact old in-memory object on write failure; the successful previous localStorage payload is not overwritten. The user gets the edited design as a share-code backup.
- **Shared-code import:** validate before mutation, then roll back the array append if persistence fails and explain that import was not saved. Never announce a successful import on a failed storage write.
- **Success:** only report saved/imported after `localStorage.setItem` returned without throwing. No new assets, allocations in rendering, save format or per-frame gameplay operations.

Regression `test/browser_creator_storage_test.js` injects a failing storage implementation around the real production JavaScript and checks quota failure, rollback, backup, successful retry, and atomic import. The existing full host suite and new quick save-recovery CI gate both execute the regression.

Manual acceptance remains needed for browser private modes, Android/iOS clipboard/prompt behavior, storage-pressure UX and reload after a quota failure.
