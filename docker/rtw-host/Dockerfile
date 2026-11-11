# Pen Pen S6: the retrowser host (its local dev server + GitHub mock, tools/rtw.mjs) and Playwright's headless
# Chromium, for the module check and the end-to-end test (tools/rtw.sh). The build context is a clean copy of
# the retrowser checkout's main (git archive, made by tools/rtw.sh image). Network only during this build;
# every container of this image runs with --network none.
FROM node:24-bookworm-slim
ENV PLAYWRIGHT_BROWSERS_PATH=/ms-playwright
COPY retrowser /rtw
WORKDIR /rtw
RUN npm ci --no-audit --no-fund && npm run build && test -f dist/tools/rtw.mjs
RUN node node_modules/playwright-core/cli.js install --with-deps chromium && chmod -R a+rX /ms-playwright
