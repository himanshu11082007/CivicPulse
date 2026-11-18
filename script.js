/**
 * CivicPulse - Real-Life Multi-Tenant Civic Resolution App
 * Interactive Geotagging, Mobile Camera, Dynamic Map Routing, and Admin Fleet Engine
 */

let allIssues = [];
let heapPriorityIssues = [];
let citizenIssues = [];
let registeredCitizens = [];
let currentSession = null; // { phone, name, role }

let dashMapInstance = null;
let fullMapInstance = null;
let routePolyline = null;
let cameraStream = null;
let deferredPrompt = null;
let autoRefreshTimer = null;

const DEPOT_COORDS = [18.5204, 73.8567];

// ---------- INITIALIZATION ----------

document.addEventListener("DOMContentLoaded", () => {
  setupDragAndDrop();
  registerServiceWorker();
  initPwaInstall();
  loadNetworkInfo();
  setupAuthInputListeners();
  checkExistingSession();

  // Background auto-refresh every 8 seconds
  if (autoRefreshTimer) clearInterval(autoRefreshTimer);
  autoRefreshTimer = setInterval(() => {
    if (!document.hidden && currentSession) {
      refreshData(true);
    }
  }, 8000);
});

// Fetch dynamic local IP and update QR code
async function loadNetworkInfo() {
  try {
    const res = await fetch("/api/network-info");
    if (!res.ok) return;
    const data = await res.json();
    if (data.url) {
      const qrEl = document.getElementById("phoneQrCode");
      if (qrEl) {
        qrEl.src = `https://api.qrserver.com/v1/create-qr-code/?size=140x140&data=${encodeURIComponent(data.url)}`;
      }
      const netLink = document.getElementById("networkLinkDisplay");
      if (netLink) {
        netLink.textContent = data.url;
      }
    }
  } catch (e) {
    console.debug("Network info fetch:", e);
  }
}

// Clear auth validation errors when user types
function setupAuthInputListeners() {
  const inputs = document.querySelectorAll(".auth-form input");
  inputs.forEach(input => {
    input.addEventListener("input", () => {
      clearAllAuthErrors();
    });
  });
}

function setAuthError(containerId, message) {
  const el = document.getElementById(containerId);
  if (!el) return;
  if (message) {
    el.textContent = message;
    el.classList.remove("hidden");
  } else {
    el.textContent = "";
    el.classList.add("hidden");
  }
}

function clearAllAuthErrors() {
  setAuthError("citizenLoginError", null);
  setAuthError("citizenSignupError", null);
  setAuthError("adminLoginError", null);
}

// Check existing session
function checkExistingSession() {
  const saved = localStorage.getItem("civicpulse_session");
  if (saved) {
    try {
      currentSession = JSON.parse(saved);
      enterAppScreen();
      return;
    } catch (e) {
      console.warn("Session error", e);
    }
  }
  showAuthScreen();
}

function showAuthScreen() {
  document.getElementById("authScreen")?.classList.remove("hidden");
  document.getElementById("appScreen")?.classList.add("hidden");
}

function enterAppScreen() {
  document.getElementById("authScreen")?.classList.add("hidden");
  document.getElementById("appScreen")?.classList.remove("hidden");

  updateUserInterface();
  if (currentSession.role === "citizen") {
    showView("report");
  } else {
    showView("dashboard");
  }
  refreshData();
}

// Update UI based on active role
function updateUserInterface() {
  const isCitizen = currentSession.role === "citizen";

  document.getElementById("citizenNav")?.classList.toggle("hidden", !isCitizen);
  document.getElementById("adminNav")?.classList.toggle("hidden", isCitizen);
  document.getElementById("mobileBottomNav")?.classList.toggle("hidden", !isCitizen);

  const roleBadge = document.getElementById("roleBadge");
  const userNameDisplay = document.getElementById("userNameDisplay");
  const userPhoneDisplay = document.getElementById("userPhoneDisplay");
  const headerRolePill = document.getElementById("headerRolePill");
  const headerUserName = document.getElementById("headerUserName");
  const sidebarTagline = document.getElementById("sidebarTagline");

  if (isCitizen) {
    if (roleBadge) roleBadge.textContent = "👤 Citizen";
    if (userNameDisplay) userNameDisplay.textContent = currentSession.name;
    if (userPhoneDisplay) userPhoneDisplay.textContent = "+91 " + currentSession.phone;
    if (headerRolePill) {
      headerRolePill.textContent = "Citizen";
      headerRolePill.className = "role-pill citizen-pill";
    }
    if (headerUserName) headerUserName.textContent = currentSession.name;
    if (sidebarTagline) sidebarTagline.textContent = "CITIZEN PORTAL";
  } else {
    if (roleBadge) roleBadge.textContent = "🛡 Municipal Admin";
    if (userNameDisplay) userNameDisplay.textContent = currentSession.name;
    if (userPhoneDisplay) userPhoneDisplay.textContent = "Municipal Corporation";
    if (headerRolePill) {
      headerRolePill.textContent = "Admin";
      headerRolePill.className = "role-pill admin-pill";
    }
    if (headerUserName) headerUserName.textContent = "Admin";
    if (sidebarTagline) sidebarTagline.textContent = "ADMIN WORKSPACE";
  }
}

// ---------- AUTHENTICATION LOGIC & VALIDATIONS ----------

function switchAuthTab(tab) {
  clearAllAuthErrors();
  document.querySelectorAll(".auth-tab-btn").forEach(b => b.classList.remove("active"));
  document.getElementById("citizenLoginForm")?.classList.add("hidden");
  document.getElementById("citizenSignupForm")?.classList.add("hidden");
  document.getElementById("adminLoginForm")?.classList.add("hidden");

  if (tab === "citizenLogin") {
    document.getElementById("tabCitizenLogin")?.classList.add("active");
    document.getElementById("citizenLoginForm")?.classList.remove("hidden");
  } else if (tab === "citizenSignup") {
    document.getElementById("tabCitizenSignup")?.classList.add("active");
    document.getElementById("citizenSignupForm")?.classList.remove("hidden");
  } else if (tab === "adminLogin") {
    document.getElementById("tabAdminLogin")?.classList.add("active");
    document.getElementById("adminLoginForm")?.classList.remove("hidden");
  }
}

async function handleCitizenLogin(e) {
  e.preventDefault();
  clearAllAuthErrors();

  const phoneInput = document.getElementById("loginPhone");
  const passInput = document.getElementById("loginPassword");
  const phone = (phoneInput?.value || "").trim();
  const password = (passInput?.value || "").trim();

  // Strict Validations
  if (!phone) {
    setAuthError("citizenLoginError", "⚠️ Please enter your 10-digit mobile number.");
    phoneInput?.focus();
    return;
  }
  if (!/^[0-9]{10}$/.test(phone)) {
    setAuthError("citizenLoginError", "⚠️ Please enter a valid 10-digit mobile number (digits only).");
    phoneInput?.focus();
    return;
  }
  if (!password) {
    setAuthError("citizenLoginError", "⚠️ Please enter your password.");
    passInput?.focus();
    return;
  }

  try {
    const res = await fetch("/api/auth/login", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ phone, password, role: "citizen" })
    });

    const data = await res.json();
    if (!res.ok) {
      setAuthError("citizenLoginError", data.error || "Login failed.");
      showToast(data.error || "Login failed");
      return;
    }

    currentSession = data.user;
    localStorage.setItem("civicpulse_session", JSON.stringify(currentSession));
    showToast(`Welcome back, ${currentSession.name}!`);
    enterAppScreen();
  } catch (err) {
    setAuthError("citizenLoginError", "Server connection error. Please make sure the CivicPulse server is running.");
    showToast("Connection error");
  }
}

async function handleCitizenSignup(e) {
  e.preventDefault();
  clearAllAuthErrors();

  const nameInput = document.getElementById("signupName");
  const phoneInput = document.getElementById("signupPhone");
  const passInput = document.getElementById("signupPassword");
  const confirmPassInput = document.getElementById("signupConfirmPassword");

  const name = (nameInput?.value || "").trim();
  const phone = (phoneInput?.value || "").trim();
  const password = (passInput?.value || "").trim();
  const confirmPassword = (confirmPassInput?.value || "").trim();

  // Strict Validations
  if (!name || name.length < 2) {
    setAuthError("citizenSignupError", "⚠️ Please enter your full name.");
    nameInput?.focus();
    return;
  }
  if (!phone) {
    setAuthError("citizenSignupError", "⚠️ Please enter your 10-digit mobile number.");
    phoneInput?.focus();
    return;
  }
  if (!/^[0-9]{10}$/.test(phone)) {
    setAuthError("citizenSignupError", "⚠️ Please enter a valid 10-digit mobile number (e.g. 9876543210).");
    phoneInput?.focus();
    return;
  }
  if (!password) {
    setAuthError("citizenSignupError", "⚠️ Please enter a password.");
    passInput?.focus();
    return;
  }
  if (password.length < 4) {
    setAuthError("citizenSignupError", "⚠️ Password must be at least 4 characters long.");
    passInput?.focus();
    return;
  }
  if (password !== confirmPassword) {
    setAuthError("citizenSignupError", "⚠️ Passwords do not match. Please re-enter.");
    confirmPassInput?.focus();
    return;
  }

  try {
    const res = await fetch("/api/auth/register", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ name, phone, password })
    });

    const data = await res.json();
    if (!res.ok) {
      setAuthError("citizenSignupError", data.error || "Sign-up failed.");
      showToast(data.error || "Sign-up failed");
      return;
    }

    currentSession = data.user;
    localStorage.setItem("civicpulse_session", JSON.stringify(currentSession));
    showToast(`Account created successfully! Welcome, ${currentSession.name}.`);
    enterAppScreen();
  } catch (err) {
    setAuthError("citizenSignupError", "Server connection error. Please make sure the CivicPulse server is running.");
    showToast("Connection error");
  }
}

async function handleAdminLogin(e) {
  e.preventDefault();
  clearAllAuthErrors();

  const userInput = document.getElementById("adminUsername");
  const passInput = document.getElementById("adminPassword");
  const username = (userInput?.value || "").trim();
  const password = (passInput?.value || "").trim();

  // Strict Validations
  if (!username) {
    setAuthError("adminLoginError", "⚠️ Please enter administrator username.");
    userInput?.focus();
    return;
  }
  if (!password) {
    setAuthError("adminLoginError", "⚠️ Please enter administrator security password / PIN.");
    passInput?.focus();
    return;
  }

  try {
    const res = await fetch("/api/auth/login", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ phone: username, password, role: "admin" })
    });

    const data = await res.json();
    if (!res.ok) {
      setAuthError("adminLoginError", data.error || "Invalid administrator credentials.");
      showToast(data.error || "Invalid Admin login");
      return;
    }

    currentSession = data.user;
    localStorage.setItem("civicpulse_session", JSON.stringify(currentSession));
    showToast("🛡 Authorized: Entered Municipal Officer Workspace");
    enterAppScreen();
  } catch (err) {
    setAuthError("adminLoginError", "Server connection error. Please verify the CivicPulse server is running.");
    showToast("Connection error");
  }
}

function logout() {
  currentSession = null;
  localStorage.removeItem("civicpulse_session");
  showToast("Logged out successfully.");
  showAuthScreen();
}

// ---------- FETCH & REFRESH DATA ----------

async function refreshData(silent = false) {
  if (!currentSession) return;

  if (currentSession.role === "citizen") {
    await loadCitizenIssues();
    renderCitizenComplaints();
  } else {
    await Promise.all([
      loadAdminAllIssues(),
      loadPriorityQueue(),
      loadRegisteredCitizens()
    ]);
    renderAdminViews();
    if (!silent) {
      setTimeout(() => initLeafletMaps(), 200);
    }
  }
}

async function loadCitizenIssues() {
  try {
    const res = await fetch(`/api/complaints?citizenPhone=${encodeURIComponent(currentSession.phone)}`);
    if (!res.ok) throw new Error("HTTP error " + res.status);
    const data = await res.json();
    citizenIssues = data.map(formatComplaintObject);
  } catch (err) {
    console.debug("Fetch citizen error:", err);
    citizenIssues = [];
  }
}

async function loadAdminAllIssues() {
  try {
    const res = await fetch("/api/complaints");
    if (!res.ok) throw new Error("HTTP error " + res.status);
    const data = await res.json();
    allIssues = data.map(formatComplaintObject);
  } catch (err) {
    console.debug("Fetch all issues error:", err);
    allIssues = [];
  }
}

async function loadPriorityQueue() {
  try {
    const res = await fetch("/api/priority-queue");
    if (!res.ok) throw new Error("HTTP error " + res.status);
    const data = await res.json();
    heapPriorityIssues = data.map(formatComplaintObject);
  } catch (err) {
    console.debug("Fetch priority queue error:", err);
    heapPriorityIssues = [];
  }
}

async function loadRegisteredCitizens() {
  try {
    const res = await fetch("/api/users");
    if (!res.ok) throw new Error("Users fetch error");
    registeredCitizens = await res.json();
    renderCitizensDirectory();
  } catch (err) {
    console.debug("Load users error:", err);
    registeredCitizens = [];
  }
}

function formatComplaintObject(c) {
  return {
    id: c.id,
    category: c.category,
    type: c.type,
    location: c.location,
    description: c.description || "",
    status: c.status,
    safety: c.safety,
    condition: c.severity,
    people: c.people,
    date: new Date((c.reportedAt || 0) * 1000).toISOString(),
    urgent: c.urgent,
    locationSensitivity: c.locationSensitivity || 1,
    emergencyOverride: c.emergencyOverride || false,
    latitude: c.latitude || 18.5204,
    longitude: c.longitude || 73.8567,
    photoUrl: c.photoUrl,
    resolvedAt: c.resolvedAt,
    resolutionNotes: c.resolutionNotes || "",
    assignedTo: c.assignedTo || "",
    citizenPhone: c.citizenPhone || "",
    citizenName: c.citizenName || "Citizen",
    backendPriority: c.priority,
    backendPriorityScore: c.priorityScore,
    aiReason: c.aiReason || "",
    aiSource: c.aiSource || ""
  };
}

// ---------- PHOTO CAPTURE & AUTOMATIC GEOTAG ----------

function setupDragAndDrop() {
  const dropzone = document.getElementById("dropzone");
  if (!dropzone) return;

  ["dragenter", "dragover"].forEach(event => {
    dropzone.addEventListener(event, e => {
      e.preventDefault();
      dropzone.classList.add("dragover");
    });
  });

  ["dragleave", "drop"].forEach(event => {
    dropzone.addEventListener(event, e => {
      e.preventDefault();
      dropzone.classList.remove("dragover");
    });
  });

  dropzone.addEventListener("drop", e => {
    if (e.dataTransfer.files.length) {
      processSelectedFile(e.dataTransfer.files[0]);
    }
  });
}

function handleFileSelect(event) {
  if (event.target.files && event.target.files.length) {
    processSelectedFile(event.target.files[0]);
  }
}

// Compress image on client side using Canvas (keeps max 1280px, ~120KB JPEG)
function compressImage(file, maxWidth = 1280, quality = 0.82) {
  return new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.onerror = reject;
    reader.onload = e => {
      const img = new Image();
      img.onerror = reject;
      img.onload = () => {
        let width = img.width;
        let height = img.height;
        if (width > maxWidth) {
          height = Math.round((height * maxWidth) / width);
          width = maxWidth;
        }
        const canvas = document.createElement("canvas");
        canvas.width = width;
        canvas.height = height;
        const ctx = canvas.getContext("2d");
        ctx.drawImage(img, 0, 0, width, height);
        resolve(canvas.toDataURL("image/jpeg", quality));
      };
      img.src = e.target.result;
    };
    reader.readAsDataURL(file);
  });
}

async function processSelectedFile(file) {
  if (!file || !file.type.startsWith("image/")) {
    showToast("Please choose a valid image file.");
    return;
  }

  showToast("Processing photo & locking GPS coordinates...");

  try {
    const dataUrl = await compressImage(file, 1280, 0.82);
    document.getElementById("previewImg").src = dataUrl;
    document.getElementById("photoUrl").value = dataUrl;

    const coords = await extractExifGps(file);
    let lat = coords ? coords.latitude : null;
    let lng = coords ? coords.longitude : null;

    if (!lat || !lng) {
      if ("geolocation" in navigator) {
        navigator.geolocation.getCurrentPosition(
          pos => applyExtractedGeotag(pos.coords.latitude, pos.coords.longitude, dataUrl),
          () => applyExtractedGeotag(18.5280, 73.8570, dataUrl),
          { timeout: 4000 }
        );
      } else {
        applyExtractedGeotag(18.5280, 73.8570, dataUrl);
      }
    } else {
      applyExtractedGeotag(lat, lng, dataUrl);
    }
  } catch (err) {
    console.error("Photo process note:", err);
    // Fallback to direct read
    const reader = new FileReader();
    reader.onload = e => {
      const dataUrl = e.target.result;
      document.getElementById("previewImg").src = dataUrl;
      document.getElementById("photoUrl").value = dataUrl;
      applyExtractedGeotag(18.5280, 73.8570, dataUrl);
    };
    reader.readAsDataURL(file);
  }
}

// Binary EXIF GPS Parser
async function extractExifGps(file) {
  try {
    const arrayBuffer = await file.arrayBuffer();
    const dataView = new DataView(arrayBuffer);
    if (dataView.getUint16(0) !== 0xFFD8) return null;

    let offset = 2;
    const length = dataView.byteLength;

    while (offset < length) {
      const marker = dataView.getUint16(offset);
      offset += 2;
      if (marker === 0xFFE1) {
        offset += 2;
        const exifHeader = String.fromCharCode(
          dataView.getUint8(offset), dataView.getUint8(offset + 1),
          dataView.getUint8(offset + 2), dataView.getUint8(offset + 3)
        );
        if (exifHeader !== "Exif") return null;

        const tiffOffset = offset + 6;
        const littleEndian = dataView.getUint16(tiffOffset) === 0x4949;
        const ifd0Offset = tiffOffset + dataView.getUint32(tiffOffset + 4, littleEndian);
        const entries = dataView.getUint16(ifd0Offset, littleEndian);

        let gpsOffset = 0;
        for (let i = 0; i < entries; i++) {
          const entryOffset = ifd0Offset + 2 + i * 12;
          const tag = dataView.getUint16(entryOffset, littleEndian);
          if (tag === 0x8825) {
            gpsOffset = tiffOffset + dataView.getUint32(entryOffset + 8, littleEndian);
            break;
          }
        }

        if (gpsOffset > 0) {
          const gpsEntries = dataView.getUint16(gpsOffset, littleEndian);
          let latRef = "N", lonRef = "E";
          let latVals = null, lonVals = null;

          for (let j = 0; j < gpsEntries; j++) {
            const entryOffset = gpsOffset + 2 + j * 12;
            const tag = dataView.getUint16(entryOffset, littleEndian);
            const valOffset = tiffOffset + dataView.getUint32(entryOffset + 8, littleEndian);

            if (tag === 1) latRef = String.fromCharCode(dataView.getUint8(entryOffset + 8));
            else if (tag === 2) {
              latVals = [
                dataView.getUint32(valOffset, littleEndian) / dataView.getUint32(valOffset + 4, littleEndian),
                dataView.getUint32(valOffset + 8, littleEndian) / dataView.getUint32(valOffset + 12, littleEndian),
                dataView.getUint32(valOffset + 16, littleEndian) / dataView.getUint32(valOffset + 20, littleEndian)
              ];
            } else if (tag === 3) lonRef = String.fromCharCode(dataView.getUint8(entryOffset + 8));
            else if (tag === 4) {
              lonVals = [
                dataView.getUint32(valOffset, littleEndian) / dataView.getUint32(valOffset + 4, littleEndian),
                dataView.getUint32(valOffset + 8, littleEndian) / dataView.getUint32(valOffset + 12, littleEndian),
                dataView.getUint32(valOffset + 16, littleEndian) / dataView.getUint32(valOffset + 20, littleEndian)
              ];
            }
          }

          if (latVals && lonVals) {
            let lat = latVals[0] + latVals[1] / 60 + latVals[2] / 3600;
            let lon = lonVals[0] + lonVals[1] / 60 + lonVals[2] / 3600;
            if (latRef === "S") lat = -lat;
            if (lonRef === "W") lon = -lon;
            return { latitude: lat, longitude: lon };
          }
        }
        return null;
      } else if ((marker & 0xFF00) === 0xFF00) {
        offset += dataView.getUint16(offset);
      } else {
        break;
      }
    }
  } catch (e) {
    console.debug("EXIF parse note:", e);
  }
  return null;
}

// Camera controls - Seamless mobile fallback for HTTP LAN
async function toggleCamera() {
  const isSecure = (location.protocol === "https:" || location.hostname === "localhost" || location.hostname === "127.0.0.1");
  const hasGetUserMedia = !!(navigator.mediaDevices && navigator.mediaDevices.getUserMedia);

  // If on mobile via plain HTTP (e.g. http://192.168.x.x), modern mobile browsers block getUserMedia.
  // HTML5 capture="environment" opens native phone camera directly and seamlessly!
  if (!isSecure || !hasGetUserMedia) {
    const nativeCam = document.getElementById("nativeCameraInput");
    if (nativeCam) {
      nativeCam.click();
      return;
    }
  }

  const video = document.getElementById("cameraVideo");
  const snapBtn = document.getElementById("snapBtn");

  if (cameraStream) {
    cameraStream.getTracks().forEach(t => t.stop());
    cameraStream = null;
    if (video) video.style.display = "none";
    if (snapBtn) snapBtn.style.display = "none";
    return;
  }

  try {
    cameraStream = await navigator.mediaDevices.getUserMedia({
      video: { facingMode: "environment" },
      audio: false
    });
    if (video) {
      video.srcObject = cameraStream;
      video.style.display = "block";
    }
    if (snapBtn) snapBtn.style.display = "block";
    showToast("Camera active. Capture photo of the civic issue.");
  } catch (err) {
    console.warn("Webcam stream unavailable, opening phone camera capture:", err);
    const nativeCam = document.getElementById("nativeCameraInput");
    if (nativeCam) {
      nativeCam.click();
    } else {
      showToast("Camera access unavailable. Please use file upload.");
    }
  }
}

function takePhotoSnapshot() {
  const video = document.getElementById("cameraVideo");
  const canvas = document.getElementById("cameraCanvas");
  if (!video || !canvas) return;

  canvas.width = video.videoWidth || 640;
  canvas.height = video.videoHeight || 480;
  const ctx = canvas.getContext("2d");
  ctx.drawImage(video, 0, 0, canvas.width, canvas.height);

  const dataUrl = canvas.toDataURL("image/jpeg", 0.82);
  document.getElementById("previewImg").src = dataUrl;
  document.getElementById("photoUrl").value = dataUrl;

  if (cameraStream) {
    cameraStream.getTracks().forEach(t => t.stop());
    cameraStream = null;
    video.style.display = "none";
    document.getElementById("snapBtn").style.display = "none";
  }

  // Get current device GPS
  if ("geolocation" in navigator) {
    navigator.geolocation.getCurrentPosition(
      pos => applyExtractedGeotag(pos.coords.latitude, pos.coords.longitude, dataUrl),
      () => applyExtractedGeotag(18.5280, 73.8570, dataUrl)
    );
  } else {
    applyExtractedGeotag(18.5280, 73.8570, dataUrl);
  }
}

function applyExtractedGeotag(lat, lng, dataUrl) {
  document.getElementById("dispLat").textContent = lat.toFixed(4);
  document.getElementById("dispLng").textContent = lng.toFixed(4);
  document.getElementById("latitude").value = lat.toFixed(5);
  document.getElementById("longitude").value = lng.toFixed(5);

  const estimatedArea = `GPS Sector [${lat.toFixed(3)}N, ${lng.toFixed(3)}E]`;
  document.getElementById("dispAddress").textContent = estimatedArea;
  document.getElementById("location").value = estimatedArea;

  document.getElementById("photoSuccessCard").classList.remove("hidden");
  showToast("✓ Photo GPS coordinates locked!");

  // Smooth scroll to Step 2
  document.getElementById("reportForm")?.scrollIntoView({ behavior: "smooth" });
}

// ---------- CITIZEN COMPLAINT SUBMISSION ----------

async function submitCitizenComplaint(e) {
  e.preventDefault();

  const photoUrl = document.getElementById("photoUrl").value;
  if (!photoUrl) {
    showToast("⚠️ Please take or upload a photo of the civic issue.");
    document.querySelector(".geotag-capture-card")?.scrollIntoView({ behavior: "smooth" });
    return;
  }

  const type = document.getElementById("type").value;
  const description = document.getElementById("description").value.trim() || `Civic issue reported via mobile camera geotag.`;
  const urgent = document.getElementById("urgent").checked;
  const lat = parseFloat(document.getElementById("latitude").value) || 18.5280;
  const lng = parseFloat(document.getElementById("longitude").value) || 73.8570;
  const location = document.getElementById("location").value || `GPS [${lat.toFixed(3)}, ${lng.toFixed(3)}]`;

  const payload = {
    type,
    location,
    description,
    latitude: lat,
    longitude: lng,
    photoUrl,
    urgent,
    citizenPhone: currentSession.phone,
    citizenName: currentSession.name
  };

  try {
    const res = await fetch("/api/complaints", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload)
    });

    if (!res.ok) {
      const err = await res.json().catch(() => ({}));
      throw new Error(err.error || "Failed to submit");
    }

    const created = await res.json();
    showToast(`✓ Complaint #CP-${created.id} submitted to Municipal Corporation!`);

    // Reset Form
    document.getElementById("reportForm").reset();
    document.getElementById("photoSuccessCard").classList.add("hidden");
    document.getElementById("photoUrl").value = "";

    await refreshData();
    showView("myComplaints");
  } catch (err) {
    console.error("Submission error:", err);
    showToast(err.message || "Submission failed. Ensure CivicPulse server is running.");
  }
}

// ---------- CITIZEN MY COMPLAINTS VIEW ----------

function renderCitizenComplaints() {
  const container = document.getElementById("citizenComplaintsList");
  if (!container) return;

  if (citizenIssues.length === 0) {
    container.innerHTML = `
      <div class="empty-state-card">
        <span class="empty-icon">📂</span>
        <h3>No complaints reported yet</h3>
        <p class="muted">You have not submitted any complaints under mobile number <b>+91 ${escapeText(currentSession.phone)}</b>.</p>
        <button class="primary" style="margin-top:14px;" onclick="showView('report')">＋ Report an Issue with Photo</button>
      </div>
    `;
    return;
  }

  const sorted = [...citizenIssues].sort((a, b) => new Date(b.date) - new Date(a.date));

  container.innerHTML = sorted.map(c => {
    const isResolved = c.status === "Resolved" || c.status === "Closed";
    const isInProgress = c.status === "In Progress";
    
    const statusClass = isResolved ? "status-resolved" : isInProgress ? "status-in-progress" : "status-submitted";
    const statusText = isResolved ? "✓ Resolved" : isInProgress ? "⚙ In Progress" : "⏳ Submitted";
    
    const reportedDate = new Date(c.date).toLocaleDateString("en-IN", {
      day: "numeric", month: "short", year: "numeric", hour: "2-digit", minute: "2-digit"
    });

    return `
      <article class="citizen-complaint-card">
        <div class="card-media">
          <img src="${c.photoUrl}" alt="Photo" class="clickable-photo" onclick="openPhotoViewer('${c.photoUrl}', '#CP-${c.id}: ${escapeText(c.type)}')">
          <span class="status-badge ${statusClass}">${statusText}</span>
          <span class="tap-hint">🔍 Tap to enlarge</span>
        </div>
        <div class="card-content">
          <div class="card-header-row">
            <h4>${escapeText(c.type)}</h4>
            <span class="complaint-id">#CP-${c.id}</span>
          </div>
          <p class="card-meta">📍 <b>Location:</b> ${escapeText(c.location)}</p>
          <p class="card-desc">${escapeText(c.description)}</p>
          <small class="muted">Reported on: ${reportedDate}</small>
          
          ${isResolved ? `
            <div class="resolution-feedback-box">
              <strong>✓ Work Completed by Municipal Crew:</strong>
              <p class="small">"${escapeText(c.resolutionNotes || "Issue inspected and resolved.")}"</p>
              ${c.assignedTo ? `<small class="muted">Officer in charge: ${escapeText(c.assignedTo)}</small>` : ""}
            </div>
          ` : isInProgress ? `
            <div class="progress-feedback-box">
              <strong>⚙ Active Field Repair:</strong>
              <p class="small">Assigned Technician: ${escapeText(c.assignedTo || "Ward Maintenance Crew")}</p>
            </div>
          ` : `
            <div class="submitted-feedback-box">
              <small class="muted">Status: Registered. Municipal department is reviewing priority ranking.</small>
            </div>
          `}

          <div class="citizen-card-actions">
            <button class="action-btn-danger" onclick="deleteComplaint(${c.id})">🗑 Delete Complaint</button>
          </div>
        </div>
      </article>
    `;
  }).join("");
}

// ---------- ADMIN VIEWS RENDERING ----------

function renderAdminViews() {
  const isResolved = c => ["Resolved", "Closed"].includes(c.status);
  const isInProgress = c => c.status === "In Progress";
  const openIssues = allIssues.filter(c => !isResolved(c));
  const resolvedIssues = allIssues.filter(isResolved);

  // Stats Counters
  const totalEl = document.getElementById("total");
  const attEl = document.getElementById("attention");
  const progEl = document.getElementById("progress");
  const resEl = document.getElementById("resolved");

  if (totalEl) totalEl.textContent = allIssues.length;
  if (attEl) attEl.textContent = openIssues.filter(c => ["Critical", "High", "URGENT"].includes(c.backendPriority)).length;
  if (progEl) progEl.textContent = allIssues.filter(isInProgress).length;
  if (resEl) resEl.textContent = resolvedIssues.length;

  // Recent rows (Dashboard)
  const recentTbody = document.getElementById("recentRows");
  if (recentTbody) {
    const recent = [...allIssues].sort((a, b) => new Date(b.date) - new Date(a.date)).slice(0, 5);
    recentTbody.innerHTML = recent.length
      ? recent.map(c => renderAdminRow(c, "recent")).join("")
      : `<tr><td colspan="7" class="text-center muted">No complaints filed by citizens yet.</td></tr>`;
  }

  renderAdminIssues();

  // Priority queue (Binary Max-Heap from C++ backend)
  const queueTbody = document.getElementById("priorityRows");
  if (queueTbody) {
    queueTbody.innerHTML = heapPriorityIssues.length
      ? heapPriorityIssues.map((c, i) => renderAdminRow(c, "queue", i)).join("")
      : `<tr><td colspan="9" class="text-center muted">All complaints resolved! Priority queue is clear.</td></tr>`;
  }

  // Resolved Issues rows (Resolved tab)
  const resTbody = document.getElementById("resolvedRows");
  if (resTbody) {
    resTbody.innerHTML = resolvedIssues.length
      ? resolvedIssues.map(c => renderAdminRow(c, "resolved")).join("")
      : `<tr><td colspan="7" class="text-center muted">No resolved issues yet. Resolved complaints will appear here.</td></tr>`;
  }

  updateMapMarkers();
}

function renderCitizensDirectory() {
  const tbody = document.getElementById("citizenDirectoryRows");
  if (!tbody) return;

  if (registeredCitizens.length === 0) {
    tbody.innerHTML = `<tr><td colspan="4" class="text-center muted">No citizen accounts registered yet.</td></tr>`;
    return;
  }

  tbody.innerHTML = registeredCitizens.map(u => {
    const regDate = u.registeredAt ? new Date(u.registeredAt * 1000).toLocaleDateString() : "Active";
    return `
      <tr>
        <td><b>👤 ${escapeText(u.name)}</b></td>
        <td>+91 ${escapeText(u.phone)}</td>
        <td>${regDate}</td>
        <td><span class="pill-badge pill-green">${u.complaintsCount} complaints</span></td>
      </tr>
    `;
  }).join("");
}

function renderAdminIssues() {
  const query = (document.getElementById("search")?.value || "").toLowerCase();
  const statusFilter = document.getElementById("statusFilter")?.value || "all";

  const filtered = allIssues.filter(c => {
    const match = `${c.id} ${c.type} ${c.category} ${c.location} ${c.citizenName} ${c.citizenPhone}`.toLowerCase().includes(query);
    const matchSt = statusFilter === "all" || c.status === statusFilter;
    return match && matchSt;
  });

  const tbody = document.getElementById("issueRows");
  if (tbody) {
    tbody.innerHTML = filtered.length
      ? filtered.map(c => renderAdminRow(c)).join("")
      : `<tr><td colspan="7" class="text-center muted">No matching complaints found.</td></tr>`;
  }
}

function renderAdminRow(c, mode = "all", rank = 0) {
  const priority = c.backendPriority || "Medium";
  const pill = `<span class="pill ${priority.toLowerCase()}">${escapeText(priority)}</span>`;
  const photo = `<img src="${c.photoUrl}" alt="Photo" class="thumb-img clickable-photo" onclick="openPhotoViewer('${c.photoUrl}', '#CP-${c.id}: ${escapeText(c.type)}', ${c.id})" title="Click to enlarge">`;
  const citizen = `<b>${escapeText(c.citizenName)}</b><br><small class="muted">+91 ${escapeText(c.citizenPhone)}</small>`;
  const issueTitle = `<b>${escapeText(c.type)}</b><br><small class="muted">#CP-${c.id} · ${escapeText(c.category)}</small>`;
  const loc = `<a href="javascript:void(0)" onclick="focusComplaintOnMap(${c.id})" class="map-link-btn" title="View Geotag Location & Route">📍 ${escapeText(c.location)}<br><small class="muted">${Number(c.latitude).toFixed(3)}, ${Number(c.longitude).toFixed(3)}</small></a>`;
  const statusPill = `<span class="status-pill status-${c.status.toLowerCase().replace(/\s+/g, '-')}">${escapeText(c.status)}</span>`;

  // Standard Action Buttons
  const resolveBtn = c.status !== "Resolved" && c.status !== "Closed"
    ? `<button class="action-btn action-btn-resolve" onclick="openResolutionModal(${c.id}, 'Resolved')" title="Mark as Resolved">✓ Resolve</button>`
    : "";
  const updateBtn = `<button class="action-btn action-btn-update" onclick="openResolutionModal(${c.id})" title="Update Status & Officer">⚙ Update</button>`;
  const mapBtn = `<button class="action-btn action-btn-map" onclick="focusComplaintOnMap(${c.id})" title="View Driving Route on Map">🗺 Route</button>`;
  const deleteBtn = `<button class="action-btn action-btn-delete" onclick="deleteComplaint(${c.id})" title="Delete Complaint">🗑 Delete</button>`;

  const actionGroup = `<div class="action-btn-group">${resolveBtn}${updateBtn}${mapBtn}${deleteBtn}</div>`;

  if (mode === "recent") {
    return `<tr><td>${photo}</td><td>${issueTitle}</td><td>${citizen}</td><td>${loc}</td><td>${c.backendPriorityScore} ${pill}</td><td>${statusPill}</td><td>${actionGroup}</td></tr>`;
  }
  if (mode === "queue") {
    const desc = c.description ? `<br><small class="muted" style="display:block;max-width:280px;white-space:normal;">"${escapeText(c.description)}"</small>` : "";
    const fullIssue = `<b>${escapeText(c.type)}</b><br><small class="muted">#CP-${c.id} · ${escapeText(c.category)}</small>${desc}`;
    const reasonText = c.aiReason
      ? `<div class="ai-reason-cell"><small>${escapeText(c.aiReason)}</small>${c.aiSource ? `<br><span class="ai-source-badge">${escapeText(c.aiSource)}</span>` : ""}</div>`
      : `<span class="muted">Standard rule scoring</span>`;
    return `<tr><td><span class="rank-badge">#${rank + 1}</span></td><td>${photo}</td><td>${fullIssue}</td><td>${citizen}</td><td>${loc}</td><td><b>${c.backendPriorityScore} / 100</b><br>${pill}</td><td>${reasonText}</td><td>${statusPill}</td><td>${actionGroup}</td></tr>`;
  }
  if (mode === "resolved") {
    const notes = c.resolutionNotes ? `<p class="res-notes">"${escapeText(c.resolutionNotes)}"</p>` : `<span class="muted">No notes</span>`;
    const officer = c.assignedTo ? `<small class="muted">By: ${escapeText(c.assignedTo)}</small>` : ``;
    const resolvedActions = `<div class="action-btn-group">${updateBtn}${mapBtn}${deleteBtn}</div>`;
    return `<tr><td>${photo}</td><td>${issueTitle}</td><td>${citizen}</td><td>${loc}</td><td>${notes}${officer}</td><td>${statusPill}</td><td>${resolvedActions}</td></tr>`;
  }

  return `<tr><td>${photo}</td><td>${issueTitle}</td><td>${citizen}</td><td>${loc}</td><td>${c.backendPriorityScore} ${pill}</td><td>${statusPill}</td><td>${actionGroup}</td></tr>`;
}

// ---------- ADMIN RESOLUTION WORKFLOW ----------

function openResolutionModal(id, preselectStatus = null) {
  const c = allIssues.find(x => x.id === id || x.id == id);
  if (!c) return;

  document.getElementById("modalComplaintId").value = c.id;
  document.getElementById("modalTitle").textContent = `Update & Resolve Complaint #CP-${c.id}`;
  document.getElementById("modalIssueDetails").innerHTML = `
    <b>Issue:</b> ${escapeText(c.type)} (${escapeText(c.category)})<br>
    <b>Citizen:</b> ${escapeText(c.citizenName)} (+91 ${escapeText(c.citizenPhone)})<br>
    <b>Location:</b> ${escapeText(c.location)}<br>
    <b>Current Status:</b> <span class="status-pill status-${c.status.toLowerCase().replace(/\s+/g, '-')}">${escapeText(c.status)}</span>
  `;

  const statusSelect = document.getElementById("modalStatusSelect");
  if (preselectStatus) {
    statusSelect.value = preselectStatus;
  } else {
    // If opening to resolve/update, default to Resolved if currently open, else keep current
    statusSelect.value = (c.status === "Submitted" || c.status === "In Progress") ? "Resolved" : c.status;
  }

  document.getElementById("modalOfficer").value = c.assignedTo || (currentSession?.name || "Ward Response Officer");
  document.getElementById("modalNotes").value = c.resolutionNotes || (statusSelect.value === "Resolved" ? "Issue inspected and repaired on-site." : "");

  document.getElementById("resolutionModal").classList.remove("hidden");
}

function closeModal() {
  document.getElementById("resolutionModal").classList.add("hidden");
}

async function submitResolution(event) {
  event.preventDefault();

  const id = Number(document.getElementById("modalComplaintId").value);
  const status = document.getElementById("modalStatusSelect").value;
  const officer = document.getElementById("modalOfficer").value.trim();
  const notes = document.getElementById("modalNotes").value.trim();

  try {
    const res = await fetch(`/api/complaints/${id}/status`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ status, officer, notes })
    });

    if (!res.ok) {
      const err = await res.json().catch(() => ({}));
      throw new Error(err.error || "Status update failed");
    }

    showToast(`✓ Complaint #CP-${id} updated to ${status}!`);
    closeModal();
    await refreshData();
  } catch (err) {
    console.error("Resolution error:", err);
    showToast("Error updating status: " + (err.message || "Check server connection."));
  }
}

// ---------- DELETE COMPLAINT ----------

async function deleteComplaint(id) {
  if (!confirm(`Are you sure you want to permanently delete complaint #CP-${id}? This action cannot be undone.`)) {
    return;
  }

  try {
    let res = await fetch(`/api/complaints/${id}`, {
      method: "DELETE"
    });

    if (!res.ok) {
      // Fallback to POST
      res = await fetch(`/api/complaints/${id}/delete`, {
        method: "POST"
      });
    }

    if (!res.ok) {
      const err = await res.json().catch(() => ({}));
      throw new Error(err.error || "Failed to delete complaint");
    }

    showToast(`🗑 Complaint #CP-${id} deleted successfully.`);
    await refreshData();
  } catch (err) {
    console.error("Delete error:", err);
    showToast("Could not delete complaint: " + (err.message || "Server error"));
  }
}

// ---------- LIGHTBOX PHOTO VIEWER ----------

function openPhotoViewer(url, title = "", complaintId = null) {
  const modal = document.getElementById("imageLightboxModal");
  const img = document.getElementById("lightboxImg");
  const caption = document.getElementById("lightboxCaption");
  const actions = document.getElementById("lightboxActions");
  if (!modal || !img) return;

  img.src = url;
  if (caption) {
    caption.textContent = title || "Civic Complaint Photo";
  }
  if (actions) {
    if (complaintId && currentSession && currentSession.role === "admin") {
      actions.innerHTML = `
        <button class="action-btn action-btn-map" onclick="closeLightboxModal(); focusComplaintOnMap(${complaintId})">
          🗺 Show Location & Route on Map
        </button>
      `;
    } else {
      actions.innerHTML = "";
    }
  }
  modal.classList.remove("hidden");
}

function openLightboxFromPreview() {
  const img = document.getElementById("previewImg");
  if (img && img.src && !img.src.endsWith("#") && img.src !== window.location.href) {
    openPhotoViewer(img.src, "Captured Issue Photo Preview");
  }
}

function closeLightboxModal() {
  document.getElementById("imageLightboxModal")?.classList.add("hidden");
}

document.addEventListener("keydown", e => {
  if (e.key === "Escape") {
    closeLightboxModal();
    closeModal();
  }
});

// ---------- LEAFLET MAPS, GEOTAG PINNING & ROUTING ----------

function initLeafletMaps() {
  if (typeof L === "undefined") return;

  const fullContainer = document.getElementById("fullMap");
  if (fullContainer && !fullMapInstance) {
    fullMapInstance = L.map("fullMap").setView(DEPOT_COORDS, 13);
    L.tileLayer("https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png", { maxZoom: 19 }).addTo(fullMapInstance);
  }

  updateMapMarkers();
}

function updateMapMarkers() {
  if (!fullMapInstance) return;

  fullMapInstance.eachLayer(layer => {
    if (layer instanceof L.Marker || layer instanceof L.Polyline) {
      fullMapInstance.removeLayer(layer);
    }
  });

  const depotIcon = L.divIcon({
    className: "custom-depot-pin",
    html: `<div style="background:#582c8b; color:white; border-radius:50%; width:28px; height:28px; display:flex; align-items:center; justify-content:center; font-weight:bold; box-shadow:0 2px 6px rgba(0,0,0,0.3); border:2px solid white;">★</div>`,
    iconSize: [28, 28]
  });
  L.marker(DEPOT_COORDS, { icon: depotIcon }).addTo(fullMapInstance).bindPopup("<b>Municipal Central Depot</b><br><small>Starting dispatch base</small>");

  allIssues.forEach(c => {
    const priorityLabel = c.backendPriority || "Medium";
    const color =
      priorityLabel === "URGENT" || priorityLabel === "Critical" ? "#cb514b" :
      priorityLabel === "High" ? "#b97a21" :
      priorityLabel === "Medium" ? "#4e7fb6" : "#42845a";

    const pinIcon = L.divIcon({
      className: "custom-issue-pin",
      html: `<div style="background:${color}; color:white; border-radius:50%; width:24px; height:24px; display:flex; align-items:center; justify-content:center; font-size:11px; font-weight:bold; border:2px solid white; box-shadow:0 2px 6px rgba(0,0,0,0.25); cursor:pointer;">${c.id}</div>`,
      iconSize: [24, 24]
    });

    const marker = L.marker([c.latitude, c.longitude], { icon: pinIcon }).addTo(fullMapInstance);
    marker.bindPopup(`
      <div style="max-width:200px;">
        <img src="${c.photoUrl}" style="width:100%; height:90px; object-fit:cover; border-radius:4px; cursor:pointer;" onclick="openPhotoViewer('${c.photoUrl}', '#CP-${c.id}: ${escapeText(c.type)}', ${c.id})">
        <b>#CP-${c.id}: ${escapeText(c.type)}</b><br>
        <small>Reported by: ${escapeText(c.citizenName)}</small><br>
        <span class="pill ${priorityLabel.toLowerCase()}">${priorityLabel} (${c.backendPriorityScore} pts)</span>
        <div style="margin-top:6px;">
          <button style="width:100%; padding:4px; font-size:11px; background:#17613a; color:white; border:none; border-radius:4px; cursor:pointer;" onclick="focusComplaintOnMap(${c.id})">🗺 Route from Depot</button>
        </div>
      </div>
    `);
  });
}

// Focus on a specific complaint's geotag and draw driving route from Municipal Depot
function focusComplaintOnMap(id) {
  showView("mapView");
  const c = allIssues.find(x => x.id === id || x.id == id);
  if (!c) return;

  setTimeout(() => {
    if (!fullMapInstance) initLeafletMaps();
    if (!fullMapInstance) return;

    fullMapInstance.invalidateSize();
    const target = [c.latitude, c.longitude];
    fullMapInstance.setView(target, 15);

    // Draw route from Municipal Central Depot to this complaint
    if (routePolyline) {
      fullMapInstance.removeLayer(routePolyline);
    }

    const routePoints = [DEPOT_COORDS, target];
    routePolyline = L.polyline(routePoints, {
      color: "#17613a",
      weight: 5,
      dashArray: "10, 8",
      opacity: 0.9
    }).addTo(fullMapInstance);

    fullMapInstance.fitBounds(routePolyline.getBounds(), { padding: [60, 60] });

    // Calculate distance and travel time
    const distKm = calculateDistanceKm(DEPOT_COORDS[0], DEPOT_COORDS[1], c.latitude, c.longitude);
    const estMins = Math.max(5, Math.round(distKm * 2.8));

    // Update Route Banner
    const banner = document.getElementById("mapRouteCard");
    const titleEl = document.getElementById("mapRouteTitle");
    const detailsEl = document.getElementById("mapRouteDetails");
    if (banner) {
      banner.classList.remove("hidden");
      if (titleEl) titleEl.textContent = `Route: Municipal Depot ➔ #CP-${c.id} (${c.type})`;
      if (detailsEl) {
        detailsEl.innerHTML = `
          <span><b>Distance:</b> ${distKm.toFixed(2)} km</span> &nbsp;•&nbsp;
          <span><b>Estimated Time:</b> ${estMins} mins</span> &nbsp;•&nbsp;
          <span><b>GPS Coordinates:</b> ${c.latitude.toFixed(4)}N, ${c.longitude.toFixed(4)}E</span> &nbsp;•&nbsp;
          <span><b>Citizen Contact:</b> +91 ${escapeText(c.citizenPhone)} (${escapeText(c.citizenName)})</span>
        `;
      }
    }

    showToast(`🗺 Plotted route to complaint #CP-${c.id} (${distKm.toFixed(2)} km)`);
  }, 250);
}

function clearSelectedMapRoute() {
  if (routePolyline && fullMapInstance) {
    fullMapInstance.removeLayer(routePolyline);
    routePolyline = null;
  }
  document.getElementById("mapRouteCard")?.classList.add("hidden");
  if (fullMapInstance) fullMapInstance.setView(DEPOT_COORDS, 13);
}

function calculateDistanceKm(lat1, lon1, lat2, lon2) {
  const R = 6371; // Earth radius in km
  const dLat = (lat2 - lat1) * Math.PI / 180;
  const dLon = (lon2 - lon1) * Math.PI / 180;
  const a = Math.sin(dLat/2) * Math.sin(dLat/2) +
            Math.cos(lat1 * Math.PI / 180) * Math.cos(lat2 * Math.PI / 180) *
            Math.sin(dLon/2) * Math.sin(dLon/2);
  const c = 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1-a));
  return R * c;
}

// Multi-Stop Dijkstra route
async function requestDispatchRoute() {
  showToast("Computing Dijkstra shortest path via C++ Graph...");
  try {
    const res = await fetch("/api/dispatch-route");
    if (!res.ok) throw new Error("API error");
    const data = await res.json();

    document.getElementById("dispatchCard").classList.remove("hidden");
    document.getElementById("routeHub").textContent = data.hub || "Municipal Central Depot";
    document.getElementById("routeWaypoints").textContent = (data.waypoints || []).join(" ➔ ");
    document.getElementById("routeDistance").textContent = (data.totalDistanceKm || 0) + " km";
    document.getElementById("routeTime").textContent = (data.estimatedMinutes || 0) + " mins";

    if (fullMapInstance) {
      if (routePolyline) fullMapInstance.removeLayer(routePolyline);
      const openIssues = allIssues.filter(x => x.status !== "Resolved" && x.status !== "Closed");
      const routePoints = [DEPOT_COORDS, ...openIssues.slice(0, 4).map(c => [c.latitude, c.longitude])];

      routePolyline = L.polyline(routePoints, { color: "#17613a", weight: 4, dashArray: "8, 8" }).addTo(fullMapInstance);
      fullMapInstance.fitBounds(routePolyline.getBounds(), { padding: [40, 40] });
    }

    showToast(`✓ Dijkstra optimal route: ${data.totalDistanceKm} km`);
  } catch (err) {
    console.debug("Dijkstra route error:", err);
  }
}

// ---------- VIEW NAVIGATION ----------

function showView(viewId) {
  document.querySelectorAll(".view").forEach(v => {
    v.classList.toggle("hidden", v.id !== viewId);
  });

  document.querySelectorAll(".nav-links button").forEach(b => {
    b.classList.remove("active");
  });

  const activeBtn = document.getElementById(`nav-${viewId.replace("View", "")}`);
  if (activeBtn) activeBtn.classList.add("active");

  document.querySelectorAll(".mobile-bottom-nav button").forEach(b => b.classList.remove("active"));
  const mobBtn = document.getElementById(`mob-${viewId}`);
  if (mobBtn) mobBtn.classList.add("active");

  const titles = {
    report: "Report an Issue with Geotag",
    myComplaints: "My Complaints",
    dashboard: "Municipal Dashboard",
    citizens: "Registered Citizen Accounts",
    issues: "All City Complaints",
    priority: "Priority Queue (Heap)",
    mapView: "Interactive Map & Dispatch Route",
    resolved: "Resolved Issues"
  };

  document.getElementById("pageTitle").textContent = titles[viewId] || viewId;
  window.scrollTo({ top: 0, behavior: "smooth" });

  if (viewId === "mapView" && fullMapInstance) {
    setTimeout(() => fullMapInstance.invalidateSize(), 200);
  }

  // Refresh data on switching to views
  if (viewId === "myComplaints" || viewId === "resolved" || viewId === "dashboard" || viewId === "issues") {
    refreshData(true);
  }
}

// ---------- PWA SERVICE WORKER & INSTALL ----------

function registerServiceWorker() {
  if ("serviceWorker" in navigator) {
    navigator.serviceWorker.register("/sw.js")
      .then(() => console.debug("CivicPulse PWA Service Worker Registered"))
      .catch(err => console.debug("SW error:", err));
  }
}

function initPwaInstall() {
  window.addEventListener("beforeinstallprompt", e => {
    e.preventDefault();
    deferredPrompt = e;
    const btn = document.getElementById("pwaInstallBtn");
    if (btn) btn.classList.remove("hidden");
  });
}

function installPwa() {
  if (deferredPrompt) {
    deferredPrompt.prompt();
    deferredPrompt.userChoice.then(() => {
      deferredPrompt = null;
      document.getElementById("pwaInstallBtn")?.classList.add("hidden");
    });
  } else {
    showToast("To install: Tap your phone browser menu (⋮ or Share) and select 'Add to Home screen'.");
  }
}

// ---------- HELPERS ----------

function escapeText(v) {
  return String(v ?? "").replace(/[&<>"']/g, c => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c]));
}

function showToast(msg) {
  const toast = document.getElementById("toast");
  if (!toast) return;
  toast.textContent = msg;
  toast.style.display = "block";
  clearTimeout(showToast.timer);
  showToast.timer = setTimeout(() => {
    toast.style.display = "none";
  }, 3200);
}
