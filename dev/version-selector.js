document.addEventListener("DOMContentLoaded", async () => {
  const path = window.location.pathname.split("/").filter(Boolean);
  if (path.length < 2 || path[0] !== "STM32LoRaWAN") {
    return;
  }

  const siteRoot = `${window.location.origin}/STM32LoRaWAN/`;
  let response;
  try {
    response = await fetch(`${siteRoot}versions.json`);
  } catch {
    return;
  }
  if (!response.ok) {
    return;
  }

  const versions = await response.json();
  const selector = document.getElementById("doc-version");
  const selectorContainer = document.getElementById("doc-version-selector");
  const currentVersion = path[1];

  for (const entry of versions) {
    const names = [entry.version, ...entry.aliases];
    for (const name of names) {
      const option = document.createElement("option");
      option.value = name;
      option.textContent = name === "latest" ? `latest (${entry.version})` : name;
      option.selected = name === currentVersion;
      selector.add(option);
    }
  }

  selector.addEventListener("change", () => {
    const pagePath = path.slice(2).map(encodeURIComponent).join("/");
    const target = `${siteRoot}${encodeURIComponent(selector.value)}/${pagePath}`;
    window.location.assign(`${target}${window.location.search}${window.location.hash}`);
  });
  selectorContainer.hidden = false;
});