"use strict";
/* OpenH910P v0.6 native JSON client. No XML parser, OEM CGI, CDN or build service. */
const $ = (id) => document.getElementById(id);
const API = Object.freeze({
  health: "/api/v1/health",
  device: "/api/v1/device",
  capabilities: "/api/v1/capabilities",
  status: "/api/v1/ups/status",
  measurements: "/api/v1/ups/measurements",
  outlets: "/api/v1/ups/outlets",
  events: "/api/v1/events"
});
let busy = false;
let controller = null;
let poller = null;

async function getJson(path, signal) {
  const response = await fetch(path, {
    method: "GET", headers: { Accept: "application/json" },
    cache: "no-store", credentials: "same-origin", signal
  });
  if (!response.ok) throw new Error(`HTTP ${response.status} from ${path}`);
  if (!(response.headers.get("Content-Type") || "").includes("application/json")) {
    throw new Error(`Unexpected response format from ${path}`);
  }
  const data = await response.json();
  if (data?.schemaVersion !== 1 || data?.source !== "simulator" || data?.simulated !== true) {
    throw new Error("Response does not match the supported development simulator contract");
  }
  return data;
}

function show(id, value) {
  const node = $(id);
  if (node) node.textContent = String(value);
}

function int(value, min, max) {
  return Number.isInteger(value) && value >= min && value <= max ? value : null;
}

function voltage(entry) {
  if (!entry || entry.unit !== "dV") return "—";
  const value = int(entry.valueDecivolts, 0, 3000);
  return value === null ? "—" : (value / 10).toFixed(1);
}

function resetReadings() {
  for (const node of document.querySelectorAll("[data-reading]")) node.textContent = "—";
  $("battery-bar").style.width = "0%";
  show("battery-caption", "Simulated status unavailable");
  show("source-value", "No live UPS data");
  show("outlets-body", "");
  show("event-list", "");
}

function paintStatus(data, measures) {
  const battery = int(data.batteryPercent, 0, 100);
  const load = int(data.loadPercent, 0, 100);
  const runtime = int(data.estimatedRuntimeSeconds, 0, 2147483647);
  show("battery-value", battery === null ? "—" : battery);
  $("battery-bar").style.width = `${battery ?? 0}%`;
  show("battery-caption", "Simulated battery condition, not real telemetry");
  show("load-value", load === null ? "—" : load);
  show("runtime-value", runtime === null ? "—" : `${Math.floor(runtime / 60)} min`);
  show("input-voltage", voltage(measures.inputVoltage));
  show("output-voltage", voltage(measures.outputVoltage));
  show("source-value", data.powerSource === "utility" ? "Example: utility power present" : "Example: battery supply");
}

function paintOutlets(outlets) {
  const parent = $("outlets-body");
  parent.replaceChildren();
  if (!Array.isArray(outlets) || !outlets.length) {
    const row = document.createElement("tr");
    const cell = document.createElement("td");
    cell.colSpan = 3;
    cell.textContent = "No simulated output segments available";
    row.append(cell);
    parent.append(row);
    return;
  }
  for (const outlet of outlets.slice(0, 10)) {
    const row = document.createElement("tr");
    const name = document.createElement("td");
    const state = document.createElement("td");
    const control = document.createElement("td");
    const button = document.createElement("button");
    name.textContent = typeof outlet.label === "string" ? outlet.label : "Unknown output";
    state.textContent = outlet.state === "on" ? "ON · SIMULATED" : "OFF · SIMULATED";
    button.textContent = "Control unavailable";
    button.type = "button";
    button.disabled = true;
    button.title = "Power commands are disabled until native hardware drivers are verified";
    control.append(button);
    row.append(name, state, control);
    parent.append(row);
  }
}

function paintEvents(events) {
  const list = $("event-list");
  list.replaceChildren();
  if (!Array.isArray(events) || !events.length) {
    const item = document.createElement("li");
    item.textContent = "No simulated events";
    list.append(item);
    return;
  }
  for (const event of events.slice(0, 30)) {
    const item = document.createElement("li");
    const number = document.createElement("span");
    const text = document.createElement("span");
    number.className = "event-number";
    number.textContent = `EVENT ${String(event.id ?? "—").padStart(2, "0")}`;
    text.textContent = typeof event.message === "string" ? event.message : "Unknown event";
    item.append(number, text);
    list.append(item);
  }
}

function paintConnection(healthy, message) {
  const mark = $("connection-mark");
  mark.classList.toggle("connected", healthy);
  mark.classList.toggle("failed", !healthy);
  show("api-state", message);
}

async function refresh() {
  if (busy) return;
  busy = true;
  $("refresh").disabled = true;
  $("main").setAttribute("aria-busy", "true");
  controller?.abort();
  controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), 4500);
  try {
    const entries = await Promise.all(Object.entries(API).map(async ([key, path]) =>
      [key, await getJson(path, controller.signal)]));
    const responses = Object.fromEntries(entries);
    if (responses.health.status !== "ok" || responses.device.hardwareConnection !== "none" ||
        responses.capabilities.physicalTelemetry !== false || responses.capabilities.outletControl !== false) {
      throw new Error("Unexpected hardware capability in simulator mode");
    }
    paintStatus(responses.status, responses.measurements);
    paintOutlets(responses.outlets.outlets);
    paintEvents(responses.events.events);
    show("firmware-value", responses.device.firmwareVersion || "Unknown");
    show("hardware-value", "Not connected");
    const now = new Date().toLocaleTimeString([], { hour: "2-digit", minute: "2-digit", second: "2-digit" });
    show("last-updated", `Updated ${now} · sample data`);
    paintConnection(true, "Local simulator connected");
  } catch (error) {
    resetReadings();
    show("last-updated", "Last request failed");
    paintConnection(false, error.name === "AbortError" ? "Simulator request timed out" : "Local simulator unavailable");
    console.error("OpenH910P simulator read failed:", error);
  } finally {
    clearTimeout(timeout);
    busy = false;
    $("refresh").disabled = false;
    $("main").removeAttribute("aria-busy");
  }
}

/* Read-only catalog is loaded once from the real C metadata engine. It has
 * 302 field definitions and deliberately no password/configuration VALUES. */
let catalog = [];
let catalogBusy = false;

function paintCatalog() {
  const search = $("schema-search").value.trim().toLowerCase();
  const matches = catalog.filter(field =>
    [field.name, field.group, field.type, field.access].some(value =>
      typeof value === "string" && value.toLowerCase().includes(search)));
  const body = $("schema-body");
  body.replaceChildren();
  for (const field of matches.slice(0, 40)) {
    const row = document.createElement("tr");
    const identifier = document.createElement("td");
    const group = document.createElement("span");
    group.className = "schema-group";
    group.textContent = field.group;
    identifier.append(group, document.createTextNode(field.name));
    const type = document.createElement("td");
    type.textContent = field.type + (field.unit ? ` (${field.unit})` : "");
    const access = document.createElement("td");
    access.textContent = field.access;
    row.append(identifier, type, access);
    body.append(row);
  }
  if (matches.length === 0) {
    const row = document.createElement("tr");
    const cell = document.createElement("td");
    cell.colSpan = 3;
    cell.textContent = "No matching fields. Try a different search.";
    row.append(cell);
    body.append(row);
  }
  show("schema-count", `Showing ${Math.min(40, matches.length)} of ${matches.length} matching definitions · ${catalog.length} total · metadata only`);
}

async function loadCatalog() {
  if (catalogBusy) return;
  catalogBusy = true;
  $("schema-retry").disabled = true;
  show("schema-count", "Loading native JSON metadata");
  const abort = new AbortController();
  const timeout = setTimeout(() => abort.abort(), 7000);
  try {
    const info = await getJson("/api/v1/configuration/fields", abort.signal);
    if (info.total !== 302 || info.pageSize !== 16 || info.pages !== 19 || info.configurationWritable !== false) {
      throw new Error("Unexpected native configuration metadata contract");
    }
    const pages = [];
    for (let page = 0; page < info.pages; page++) {
      const data = await getJson(`/api/v1/configuration/fields/${page}`, abort.signal);
      if (data.page !== page || data.total !== 302 || data.configurationWritable !== false ||
          !Array.isArray(data.items) || data.items.length < 1 || data.items.length > 16) {
        throw new Error("Invalid configuration metadata page");
      }
      pages.push(...data.items);
    }
    if (pages.length !== 302 || new Set(pages.map(field => field.name)).size !== 302 ||
        pages.some(field => Object.hasOwn(field, "value") || Object.hasOwn(field, "default"))) {
      throw new Error("Configuration metadata is incomplete or contains live values");
    }
    catalog = pages;
    $("schema-search").disabled = false;
    paintCatalog();
  } catch (error) {
    catalog = [];
    $("schema-search").disabled = true;
    show("schema-count", "Configuration catalog unavailable; native API may be offline");
    $("schema-body").replaceChildren();
    console.error("Unable to load native field catalog:", error);
  } finally {
    clearTimeout(timeout);
    $("schema-retry").disabled = false;
    catalogBusy = false;
  }
}
$("schema-search").addEventListener("input", paintCatalog);
$("schema-retry").addEventListener("click", loadCatalog);
loadCatalog();

$("refresh").addEventListener("click", refresh);
for (const link of document.querySelectorAll(".navigation a")) {
  link.addEventListener("click", () => {
    for (const other of document.querySelectorAll(".navigation a")) {
      other.classList.toggle("current", other === link);
    }
  });
}
function schedulePolling() {
  clearInterval(poller);
  if (!document.hidden) {
    poller = setInterval(refresh, 15000);
    refresh();
  }
}
document.addEventListener("visibilitychange", schedulePolling);
schedulePolling();
