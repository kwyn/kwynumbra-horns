// Keeps the controller usable with no network — the horns are worn out of the
// house, where there is no laptop and often no signal.
//
// Network-first, cache-fallback. That ordering matters both ways: online you
// always get the freshly pushed version (a cache-first worker would strand you
// on an old build the same way the githack CDN cache does), and offline you get
// the last copy that loaded. No version bumping to remember on deploy.
const CACHE = 'kwynumbra';

self.addEventListener('install', e => self.skipWaiting());
self.addEventListener('activate', e => e.waitUntil(self.clients.claim()));

self.addEventListener('fetch', event => {
  if (event.request.method !== 'GET') return;
  event.respondWith(
    fetch(event.request)
      .then(response => {
        // Clone before returning — a Response body can only be read once.
        const copy = response.clone();
        caches.open(CACHE).then(c => c.put(event.request, copy));
        return response;
      })
      .catch(() => caches.match(event.request))
  );
});
