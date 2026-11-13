
const KEY = "civicpulse-demo-data";
const daysAgo = n => new Date(Date.now() - n * 86400000).toISOString();

const sampleIssues = [
  {id:1001,type:"Pothole",location:"Near school",description:"Large pothole near the school entrance.",condition:4,people:120,status:"Open",date:daysAgo(5)},
  {id:1002,type:"Garbage accumulation",location:"Market area",description:"Garbage has accumulated beside the market bins.",condition:3,people:80,status:"Open",date:daysAgo(9)},
  {id:1003,type:"Electrical hazard",location:"Busy road",description:"An exposed electrical fitting needs professional review.",condition:5,people:60,status:"Open",date:daysAgo(1)},
  {id:1004,type:"Water leakage",location:"Residential area",description:"Water is leaking from a roadside pipe.",condition:3,people:35,status:"In progress",date:daysAgo(4)},
  {id:1005,type:"Broken streetlight",location:"Near hospital",description:"Streetlight is not working on the pedestrian route.",condition:3,people:45,status:"Open",date:daysAgo(7)},
  {id:1006,type:"Damaged footpath",location:"Public park",description:"Uneven paving slabs create a trip hazard.",condition:2,people:18,status:"Resolved",date:daysAgo(12)},
  {id:1007,type:"Garbage accumulation",location:"Residential area",description:"Waste has been left beside the collection point.",condition:2,people:12,status:"In progress",date:daysAgo(3)},
  {id:1008,type:"Pothole",location:"Busy road",description:"Road surface is cracked and uneven.",condition:3,people:55,status:"Resolved",date:daysAgo(15)}
];


async function loadIssues() {
  try {
    const response = await fetch("/api/complaints");

    if (!response.ok) {
      throw new Error("Failed to load complaints from the server.");
    }

    const complaints = await response.json();

    return complaints.map(c => ({
      id: c.id,
      type: c.type,
      category: c.category,
      location: c.location,
      description: c.description,
      condition: c.severity,
      people: c.people,
      status: c.status === "In Progress" ? "In progress" : c.status,
      date: new Date(c.reportedAt * 1000).toISOString(),
      urgent: c.urgent,
      safety: c.safety,
      backendPriority: c.priority,
      backendPriorityScore: c.priorityScore
    }));
  } catch (error) {
    console.error("Unable to load CivicPulse complaints:", error);
    return [];
  }
}


let issues = [];
let nextId = 1;

async function initializeIssues() {
  issues = await loadIssues();
  nextId = Math.max(0, ...issues.map(x => Number(x.id) || 0)) + 1;
  render();
}

initializeIssues();


function score(x) {
  const locationRisk = /school|busy road|hospital/i.test(x.location);
  let S = x.type === "Electrical hazard" ? 5 :
    x.type === "Pothole" && x.condition >= 4 && locationRisk ? 5 :
    x.condition >= 4 && locationRisk ? 4 :
    x.condition >= 4 ? 4 : x.condition >= 3 ? 3 : x.condition === 2 ? 2 : 1;
  const V = Number(x.condition);
  const n = Number(x.people);
  const A = n <= 5 ? 1 : n <= 20 ? 2 : n <= 50 ? 3 : n <= 200 ? 4 : 5;
  const days = Math.max(0, (Date.now() - new Date(x.date).getTime()) / 86400000);
  const T = days < 1 ? 1 : days <= 3 ? 2 : days <= 7 ? 3 : days <= 14 ? 4 : 5;
  const total = 8*S + 6*V + 4*A + 2*T;
  return {total, label:total >= 80 ? "Critical" : total >= 60 ? "High" : total >= 40 ? "Medium" : "Low"};
}

function escapeText(value) {
  return String(value ?? "").replace(/[&<>"']/g, c =>
    ({"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;","'":"&#39;"}[c]));
}

function dateText(date) {
  const days = Math.floor((Date.now() - new Date(date).getTime()) / 86400000);
  return days <= 0 ? "Today" : days === 1 ? "1 day ago" :
    days < 7 ? `${days} days ago` :
    new Date(date).toLocaleDateString("en-IN",{day:"numeric",month:"short"});
}

function issueName(x) {
  return x.type === "Pothole" ? "Road pothole" : x.type;
}


function row(x, mode = "all", rank = 0) {
  const p = score(x);

  // Prefer the priority calculated by the C++ backend.
  const priority = x.backendPriority || p.label;
  const priorityClass = priority.toLowerCase().replace(/\s+/g, "-");

  const pill = `<span class="pill ${priorityClass}">${escapeText(priority)}</span>`;
  const status = escapeText(x.status);
  const button = `<button onclick="changeStatus(${x.id})">Update</button>`;
  const name = `<b>${escapeText(issueName(x))}</b><br><small>#CP-${x.id}</small>`;

  if (mode === "recent") {
    return `<tr><td>${name}</td><td>${escapeText(x.location)}</td><td>${pill}</td><td>${status}</td><td>${button}</td></tr>`;
  }

  if (mode === "queue") {
    return `<tr><td>${rank + 1}</td><td>${name}</td><td>${escapeText(x.location)}</td><td>${x.backendPriorityScore ?? p.total} ${pill}</td><td>${status}</td></tr>`;
  }

  return `<tr><td>${name}</td><td>${escapeText(x.location)}</td><td>${pill}</td><td>${status}</td><td>${button}</td></tr>`;
}


function renderIssues() {
  const query = (document.querySelector("#search")?.value || "").toLowerCase();
  const found = issues.filter(x =>
    `${x.id} ${x.type} ${x.location} ${x.description}`.toLowerCase().includes(query));
  document.querySelector("#issueRows").innerHTML = found.length ?
    found.map(x => row(x)).join("") :
    `<tr><td colspan="5">No matching issues found.</td></tr>`;
}


function render() {
  const isResolved = x =>
    ["Resolved", "Closed"].includes(x.status);

  const isInProgress = x =>
    ["In Progress", "In progress"].includes(x.status);

  const open = issues.filter(x => !isResolved(x));
  const resolved = issues.filter(isResolved);

  document.querySelector("#total").textContent = issues.length;

  document.querySelector("#attention").textContent =
    open.filter(x =>
      ["Critical", "High", "URGENT"].includes(
        x.backendPriority || score(x).label
      )
    ).length;

  document.querySelector("#progress").textContent =
    issues.filter(isInProgress).length;

  document.querySelector("#resolved").textContent = resolved.length;

  document.querySelector("#recentRows").innerHTML =
    [...issues]
      .sort((a, b) => new Date(b.date) - new Date(a.date))
      .slice(0, 5)
      .map(x => row(x, "recent"))
      .join("") ||
    `<tr><td colspan="5">No issues yet.</td></tr>`;

  renderIssues();

  const queue = [...open].sort(
    (a, b) => (b.backendPriorityScore ?? score(b).total) -
              (a.backendPriorityScore ?? score(a).total)
  );

  document.querySelector("#priorityRows").innerHTML =
    queue.map((x, i) => row(x, "queue", i)).join("") ||
    `<tr><td colspan="5">No open issues.</td></tr>`;

  document.querySelector("#resolvedRows").innerHTML =
    resolved.map(x => row(x)).join("") ||
    `<tr><td colspan="5">No resolved issues yet.</td></tr>`;

  const navCount = document.querySelector("#navCount");
  if (navCount) {
    navCount.textContent = issues.length;
  }

  save();
}


function save() {
  try { localStorage.setItem(KEY, JSON.stringify(issues)); }
  catch (error) { console.warn("Unable to save browser data.", error); }
}

function showView(name) {
  document.querySelectorAll(".view").forEach(v =>
    v.classList.toggle("hidden", v.id !== name));
  document.querySelector("#pageTitle").textContent =
    ({dashboard:"Dashboard",issues:"All issues",report:"Report an issue",
      priority:"Priority queue",resolved:"Resolved issues"})[name] || name;
  window.scrollTo({top:0,behavior:"smooth"});
}


async function changeStatus(id) {
  const x = issues.find(item => Number(item.id) === Number(id));
  if (!x) return;

  // Match the status values accepted by the C++ backend.
  const nextStatus =
    x.status === "Submitted" || x.status === "Open" ? "In Progress" :
    x.status === "In Progress" || x.status === "In progress" ? "Resolved" :
    "Submitted";

  try {
    const response = await fetch(`/api/complaints/${id}/status`, {
      method: "PATCH",
      headers: {
        "Content-Type": "application/json"
      },
      body: JSON.stringify({ status: nextStatus })
    });

    const result = await response.json();

    if (!response.ok) {
      throw new Error(result.error || "Unable to update complaint status.");
    }

    // Reload the saved data from the C++ backend.
    issues = await loadIssues();
    nextId = Math.max(0, ...issues.map(item => Number(item.id) || 0)) + 1;

    render();
    showToast(`Issue #CP-${id} updated to ${nextStatus}.`);
  } catch (error) {
    console.error("Status update failed:", error);
    showToast(error.message || "Unable to update status. Please try again.");
  }
}


function showToast(message) {
  const toast = document.querySelector("#toast");
  toast.textContent = message;
  toast.style.display = "block";
  clearTimeout(showToast.timer);
  showToast.timer = setTimeout(() => toast.style.display = "none", 2800);
}


document.querySelector("#reportForm").addEventListener("submit", async event => {
  event.preventDefault();

  const type = document.querySelector("#type").value;
  const location = document.querySelector("#location").value.trim();
  const description = document.querySelector("#description").value.trim();
  const condition = Number(document.querySelector("#condition").value);
  const people = Number(document.querySelector("#people").value);

  if (!type || !location || !description ||
      !condition || people < 1) {
    showToast("Please complete all required fields.");
    return;
  }

  const payload = {
    category: type,
    type: type,
    location: location,
    description: description,
    safety: condition,
    severity: condition,
    people: people,
    urgent: false
  };

  try {
    const response = await fetch("/api/complaints", {
      method: "POST",
      headers: {
        "Content-Type": "application/json"
      },
      body: JSON.stringify(payload)
    });

    const result = await response.json();

    if (!response.ok) {
      throw new Error(result.error || "Unable to submit complaint.");
    }

    // Reload the authoritative records from the C++ backend.
    issues = await loadIssues();
    nextId = Math.max(0, ...issues.map(x => Number(x.id) || 0)) + 1;

    render();
    event.target.reset();
    showView("issues");
    showToast(`Report #CP-${result.id} added successfully.`);
  } catch (error) {
    console.error("Complaint submission failed:", error);
    showToast(error.message || "Unable to submit complaint. Please try again.");
  }
});
