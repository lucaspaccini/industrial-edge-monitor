# Continuous Integration with GitHub Actions

This guide describes the workflow implemented in [`.github/workflows/ci.yml`](../.github/workflows/ci.yml). It documents repository verification only: installation and manual procedures belong in [Setup and deployment](setup.md), container behavior belongs in [Docker and Docker Compose](docker.md), and component relationships belong in [Architecture](architecture.md).

## Purpose

Continuous Integration (CI) runs repeatable checks whenever changes are pushed or proposed through a pull request. It provides an external, clean environment that can reveal undeclared dependencies, untracked files, implicit local configuration and path assumptions that may remain hidden on a developer machine.

Local checks and GitHub-hosted checks serve different purposes:

- local checks provide fast feedback before a push;
- GitHub Actions checks the committed revision on fresh runners using the versioned workflow;
- a successful local run increases confidence but does not create a GitHub status check;
- for any pushed revision, its successful GitHub Actions run is the final repository gate.

The workflow implementation is complete, and equivalent checks passed locally during Sprint 14. It does not deploy software.

CI is distinct from later release stages:

- **Continuous Integration** builds and verifies changes frequently;
- **Continuous Delivery** additionally prepares a releasable artifact or release candidate for a controlled manual deployment;
- **Continuous Deployment** automatically deploys every qualifying change to a target environment.

Sprint 14 implements CI only. No delivery pipeline, deployment target or deployment credentials are configured.

## GitHub Actions terminology

| Term | Meaning |
| --- | --- |
| Workflow | The complete automation definition named `Continuous Integration`. |
| Workflow file | The YAML source at `.github/workflows/ci.yml`. |
| Event | Repository activity, such as `push` or `pull_request`, that can trigger a workflow. |
| Trigger | The `on` configuration mapping events to workflow runs. |
| Workflow run | One execution of the workflow for a particular event and revision. |
| Job | An isolated group of steps executed on one runner or in one job container. This workflow has `backend`, `frontend`, `firmware` and `containers`. |
| Step | One ordered operation within a job, such as checkout, dependency installation or a test command. |
| Action | A reusable GitHub Actions component referenced with `uses`, such as `actions/checkout@v7`. |
| Runner | The compute environment that executes a job. |
| GitHub-hosted runner | A temporary runner created and managed by GitHub, here based on `ubuntu-24.04`. |
| Job container | A container in which all steps of a job execute while GitHub still manages the host runner. The current workflow does not configure a job-level container; the firmware build action instead launches its own Docker container for one step. |
| Checkout | The step that downloads the repository revision into the job workspace. |
| Environment variable | A named value supplied to a workflow or process, such as `APP_ENV=test`. |
| Secret | An encrypted GitHub value intended for sensitive data. The current jobs require no application secrets. |
| Cache | Reusable dependency data that accelerates future runs. It is an optimization and can be recreated. |
| Artifact | Output uploaded from a run for later inspection or download. The current workflow uploads no artifacts. |
| Status check | The pass, fail, cancelled or pending result associated with a commit or pull request. |
| Timeout | Maximum job duration before GitHub stops it. Each job defines one. |
| Concurrency group | A key used to group runs that should supersede each other. |
| `cancel-in-progress` | A rule that cancels an older running workflow in the same concurrency group when a newer one starts. |

Cache and artifact are not interchangeable. The Python and Node setup actions cache package downloads to make later jobs faster. A cache is not a release output and may be evicted. An artifact is an explicitly uploaded build result retained with a run. This workflow does not publish firmware binaries, container archives, reports or release artifacts.

## Workflow triggers

The workflow declares:

```yaml
on:
  push:
  pull_request:
```

With no branch or path filters:

- a push to a branch or tag can start a workflow run;
- opening or updating a pull request starts the applicable pull-request run;
- later pushes create runs for their newer revisions;
- a local commit that has not been pushed cannot be executed by GitHub Actions because GitHub has not received it.

The workflow-level concurrency key is:

```yaml
concurrency:
  group: ci-${{ github.workflow }}-${{ github.ref }}
  cancel-in-progress: true
```

Runs for the same workflow and Git reference share a group. A newer run cancels an older in-progress run for that reference, avoiding full verification of a commit that has already been superseded. Runs for different references can continue independently.

## Permissions and security

The workflow requests:

```yaml
permissions:
  contents: read
```

Jobs can read repository contents for checkout but do not receive unnecessary repository write permission. They do not publish packages, create releases, modify pull requests or deploy.

Current jobs require no Wi-Fi credentials, MQTT passwords, API tokens or deployment keys. If future jobs need sensitive values, those values must be stored in GitHub Actions Secrets or an appropriate environment-level secret store and referenced at runtime. They must not be committed to the repository or written directly into the workflow.

`NEXT_PUBLIC_API_URL` is not a secret. Every `NEXT_PUBLIC_*` value is embedded in browser code and must be treated as public.

Supply-chain references have different strengths:

- a package or image version tag selects a named release but the tag can be mutable;
- a container digest identifies immutable image content;
- a GitHub Action commit SHA identifies immutable action source;
- a major action reference such as `actions/checkout@v7` is convenient but is not SHA-pinned.

The current workflow uses version references for actions, and the Espressif action selects its ESP-IDF Docker image by version tag. It does not claim action SHA pinning or image digest pinning.

## Runner lifecycle and isolation

For each trigger:

1. GitHub receives the pushed revision or pull-request event.
2. GitHub creates temporary runner environments for runnable jobs.
3. Each job receives a separate filesystem and process environment.
4. `actions/checkout` places the requested repository revision in that job's workspace.
5. The job configures its runtime and installs or restores dependencies.
6. Steps execute in order inside that job.
7. GitHub retains workflow logs and job results according to repository retention settings.
8. The temporary runner is destroyed after the job completes.

Jobs do not automatically share files, processes, databases or containers. They may run in parallel because the workflow declares no inter-job `needs` dependencies. Sharing output would require an explicit cache, artifact or external service. No job accesses the developer's local `data/telemetry.db`, and shutting down the developer computer after a successful push does not stop GitHub-hosted runners.

## Workflow-level controls

All jobs use a GitHub-hosted `ubuntu-24.04` runner and none declares a job-level container. Backend and frontend commands run directly on their runner host. The firmware build step invokes Espressif's action, which starts the selected official ESP-IDF image through Docker. The container job invokes Docker Compose directly on its runner.

Configured timeouts are:

| Job ID | GitHub display name | Timeout |
| --- | --- | --- |
| `backend` | `Backend tests` | 10 minutes |
| `frontend` | `Frontend checks` | 15 minutes |
| `firmware` | `Firmware build` | 30 minutes |
| `containers` | `Container build and smoke test` | 25 minutes |

A timeout is a failed job condition, not a retry policy. The workflow does not retry failed steps automatically.

## Backend job

The `backend` job runs on `ubuntu-24.04` with:

```text
APP_ENV=test
LOG_LEVEL=WARNING
DATABASE_PATH=/tmp/industrial-edge-monitor-tests/telemetry.db
```

It then:

1. checks out the repository with `actions/checkout@v7`;
2. installs Python `3.14.4` with `actions/setup-python@v7`;
3. enables the setup action's pip cache using `requirements.txt` and `requirements-runtime.txt` as dependency keys;
4. installs test and application dependencies from `requirements.txt`;
5. runs `python -m pytest -q`.

The `/tmp` database path starts outside the checkout and does not depend on a pre-existing development database. Repository tests use pytest temporary directories and initialize only the schema/data required by each test. The workflow-level path is also a safe fallback for any code that reads the global settings directly. This combination verifies missing-parent creation, migration behavior and test isolation on a fresh runner.

## Frontend job

The `frontend` job runs on `ubuntu-24.04`, has a 15-minute timeout and sets `frontend/` as the default working directory for shell steps. It supplies:

```text
NEXT_PUBLIC_API_URL=http://127.0.0.1:8000
```

It then:

1. checks out the repository;
2. installs Node.js `22.22.1` with `actions/setup-node@v7`;
3. enables the npm download cache keyed from `frontend/package-lock.json`;
4. runs `npm ci`;
5. runs `npm test`;
6. runs `npm run lint`;
7. runs `npm run build`.

`npm ci` is used instead of `npm install` because CI must reproduce the committed lockfile graph and fail if `package.json` and `package-lock.json` disagree. The graph currently contains Next.js `16.2.12`. The public API URL is syntactically valid and available to the build; no live API is required for static compilation.

## Firmware job

The `firmware` job runs on the GitHub-hosted `ubuntu-24.04` runner. It does not declare a job-level container or execute `idf.py` directly from the runner environment. After checkout, it delegates the build to the official Espressif action:

```yaml
- name: Build ESP32 firmware
  uses: espressif/esp-idf-ci-action@v1
  with:
    esp_idf_version: v6.0.2
    target: esp32
    path: firmware
```

`espressif/esp-idf-ci-action@v1` invokes the official `espressif/idf:v6.0.2` image through Docker for that build step. The action runs the ESP-IDF build for target `esp32` in the repository's `firmware` directory, so `idf.py` is provided by the selected image rather than assumed to exist on the GitHub runner.

[`firmware/dependencies.lock`](../firmware/dependencies.lock) records the IDF target and managed-component resolution, including `espressif/mqtt` 1.0.0, so a clean firmware build uses the versioned component graph.

This job verifies compilation only. It does not:

- flash an ESP32;
- open a serial port or monitor;
- access an ESP32-WROOM-32U or BME280;
- test real Wi-Fi or MQTT connectivity from firmware;
- exercise GPIO wiring or machine-status input;
- perform hardware fault injection.

## Container job

The `containers` job performs the broadest repository check. It runs on `ubuntu-24.04` with a 25-minute timeout and sets:

```yaml
APP_ENV: production
LOG_LEVEL: INFO
NEXT_PUBLIC_API_URL: http://127.0.0.1:8000
COMPOSE_PROJECT_NAME: iem-ci-${{ github.run_id }}-${{ github.run_attempt }}
```

The resulting unique Compose project name isolates containers, network and named volumes from other runs.

The job:

1. checks out the repository;
2. runs `docker compose config --quiet` to validate the model without printing resolved configuration;
3. runs `docker compose build --pull` to build the backend and frontend while checking for newer content behind configured base-image tags;
4. installs a shell `trap` that always performs project-scoped cleanup;
5. starts the stack with `docker compose up --detach --wait --wait-timeout 180`;
6. requires successful HTTP responses from API `/health` and frontend `/`;
7. polls collector logs for `Subscribed to legacy and per-device topics` because collector has no Docker health check;
8. inspects API and collector mounts and requires the same named volume at `/data`;
9. publishes a known legacy MQTT telemetry message through Mosquitto;
10. polls the API until the corresponding `legacy-device` sample is returned;
11. captures the inserted telemetry ID;
12. stops and removes API and collector while retaining the named volume;
13. recreates API and collector and rechecks the shared mount;
14. queries the API again and requires the same telemetry ID and expected sample values;
15. removes the isolated containers, network and volumes when the script exits.

On failure, the trap first prints `docker compose ps` and the project logs, then runs:

```bash
docker compose down --volumes --remove-orphans
```

Because `COMPOSE_PROJECT_NAME` is unique to the run, cleanup is limited to the resources created for that CI project. It does not use a developer database or an unrelated Compose project.

Checking the exact telemetry ID is stronger than checking field equality alone. It proves the post-recreation query found the same persisted row rather than a newly inserted row with the same timestamp and measurements.

## Smoke test classification

The container job is an integration test of the assembled four-service stack and a smoke test of its primary ingestion path. It verifies image construction, startup dependencies, health checks, MQTT ingestion, collector processing, shared SQLite access, API retrieval and persistence across container recreation.

It is not a complete system acceptance or security test. It does not verify:

- every API route or validation branch;
- all dashboard states and interactions;
- a real browser;
- load, throughput or resource limits;
- long-duration availability;
- offensive security properties;
- physical firmware, sensors or networking;
- multi-host failover or storage.

Container concepts and the corresponding manual smoke sequence are described in [Docker and Docker Compose](docker.md#smoke-test) and [Setup and deployment](setup.md#mqtt-smoke-test).

## Reading results on GitHub

After a push or pull request, open the repository's **Actions** tab, select the `Continuous Integration` workflow and choose the run for the relevant commit.

- A green workflow means every required job in that run completed successfully.
- A red workflow means at least one job failed; open the failed job and then the first failed step.
- A successful job confirms all of its steps passed on that runner.
- A failed job stops at or after its first failing step; later jobs may still finish because jobs run independently.
- A cancelled job is not a passing result. It may have been superseded by `cancel-in-progress` or cancelled manually.
- A timeout means the job exceeded its declared maximum duration; inspect the last active step and preceding logs.
- Step logs contain command output and are the first source for diagnosis.
- GitHub permits rerunning failed jobs or the complete workflow when the user has the necessary repository permission. A rerun uses the same commit but current external services and caches may differ.
- The repository commit and pull-request pages show the combined status checks associated with the revision.

The workflow file existing in a local checkout does not prove an external run. For any pushed revision, completion of all four GitHub-hosted jobs is the final repository gate. Sprint 14's equivalent commands passed locally; the first pushed revision must still be confirmed in GitHub Actions.

## Local reproduction

Run backend checks from the repository root:

```bash
pytest -q
```

Run frontend checks:

```bash
cd frontend
npm ci
npm test
npm run lint
NEXT_PUBLIC_API_URL=http://127.0.0.1:8000 npm run build
cd ..
```

Run the firmware build from the repository root with ESP-IDF 6.0.2 exported:

```bash
idf.py -C firmware build
```

Validate, build and start the container stack from the repository root:

```bash
docker compose config --quiet
docker compose build
docker compose up -d --wait --wait-timeout 180
```

Use a unique Compose project and isolated volumes when reproducing the CI smoke logic rather than reusing development data. The detailed procedure and cleanup boundary are in [Docker and Docker Compose](docker.md#smoke-test).

Passing these commands locally increases confidence and shortens feedback time. It does not replace execution on GitHub's clean runner environments.

## Failure scenarios

| Scenario | First useful evidence |
| --- | --- |
| Tests pass locally but fail on GitHub | Open the failed job and compare runtime versions, working directory, environment variables and the first traceback with the local command. |
| A required local file was never versioned | Check the checkout step and the first command reporting a missing path. Confirm with `git status --short`; do not make CI depend on ignored local files. |
| Code uses an undeclared dependency | Installation may pass but import/build fails on the clean runner. Inspect the first `ModuleNotFoundError`, package resolution error or compiler error and update the appropriate manifest and lockfile. |
| Path case differs | Linux reports the first missing import or file. Match path spelling exactly; a case-insensitive local filesystem can hide this issue. |
| Environment variable is missing or invalid | Inspect the first Pydantic validation, Dockerfile argument check or frontend build error. Add only required non-sensitive values at the narrowest job scope. |
| Lockfile is stale | `npm ci` reports the manifest/lock mismatch. Regenerate and review `package-lock.json` locally rather than replacing `npm ci` with `npm install`. |
| Firmware build is not reproducible | Inspect the component-manager and CMake output before the compiler failure. Check ESP-IDF version, target, manifests and `dependencies.lock`. |
| Health check times out | In the container job, inspect the automatically printed `docker compose ps` and service logs. Find the first dependency or health command that failed. |
| Container smoke command fails | The cleanup trap prints state and all project logs before removal. Start with the first service error preceding cleanup, not the cleanup messages themselves. |
| Registry or dependency service is temporarily unavailable | Build/install logs show DNS, timeout or HTTP failures. A rerun may confirm a transient outage; do not change dependency versions without evidence. |
| Cache appears stale | Determine whether the failure occurs before or after dependency installation. Setup-action caches store downloads, not authoritative source; lockfiles and clean installation remain authoritative. |
| Workflow YAML is invalid | GitHub may reject the workflow before jobs start. Inspect the Actions annotation for the exact file and line; local `docker compose config` cannot validate GitHub Actions YAML semantics. |

## Current limitations and future evolution

The current unpushed revision has only local equivalent verification. Its first positive GitHub Actions run after push remains the external repository gate.

Future CI and release work includes:

- branch protection requiring the CI status before merge;
- a workflow badge after a successful repository run exists;
- code coverage collection and reporting;
- explicit type checking;
- pre-commit hooks for faster local feedback;
- uploaded, versioned firmware artifacts;
- a Software Bill of Materials;
- automated dependency and container scanning;
- GitHub Action pinning by commit SHA;
- container base-image and job-image pinning by digest;
- Continuous Delivery with controlled release artifacts;
- controlled deployment to an explicitly selected real target.

None of these capabilities is currently implemented. In particular, the workflow has no deployment job, target credentials, environment promotion or automatic rollout.
