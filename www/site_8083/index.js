
// ============================================================================
// WEBSERV OS v0.42 - Minimal modification version, unified event binding + debugging
// ============================================================================

const BASE_WIDTHS = {
    magic_sm: 700,
    task_man: 360,
    webcam: 420,
    jine: 340,
    terminal: 500
};

const STATE = {
    windows: [
        { id: 'magic_sm', title: 'Magic Smoke', type: 'magic', x: 250, y: 120, isOpen: true, z: 15, width: 700, height: 480 },
        { id: 'task_man', title: 'Task Man', type: 'files', x: 50, y: 50, isOpen: false, z: 10, width: 360, height: 400 },
        { id: 'webcam', title: 'webcam', type: 'docs', x: 100, y: 150, isOpen: false, z: 11, width: 420, height: 480 },
        { id: 'jine', title: 'JINE', type: 'chat', x: 150, y: 200, isOpen: false, z: 12, width: 340, height: 400 },
        { id: 'terminal', title: 'System Terminal', type: 'terminal', x: 400, y: 50, isOpen: false, z: 16, width: 500, height: 350 },
    ],
    files: [],
    docs: [],
    activeDoc: null,
    activeDocContent: '',
    demoContent: '',
    uploadMessage: null,
    uploadMessageType: null,
    terminalHistory: ["Welcome to Webserv OS v0.42", "Kernel status: nominal", "Type 'help' for available commands."],
    chatHistory: JSON.parse(localStorage.getItem('webserv_chat_v3') || '[{"role": "assistant", "text": "system online. how can i help today? (〃^ω^〃)"}]')
};

const ICONS = [
    { id: 'magic_sm', label: 'about', img: 'https://api.iconify.design/pixelarticons:zap.svg' },
    { id: 'task_man', label: 'files', img: 'https://api.iconify.design/pixelarticons:folder.svg' },
    { id: 'webcam', label: 'docs', img: 'https://api.iconify.design/pixelarticons:book-open.svg' },
    { id: 'jine', label: 'jine', img: 'https://api.iconify.design/pixelarticons:message.svg' },
    { id: 'terminal', label: 'terminal', img: 'https://api.iconify.design/pixelarticons:code.svg' }
];

window.addEventListener('DOMContentLoaded', () => init());

async function init() {
    await Promise.all([
        fetchServerFiles(),
        fetchDocsMetadata()
    ]);
    renderIcons();
    renderWindows();
    renderTaskbar();
    updateClock();
    setInterval(updateClock, 1000);

    const startBtn = document.getElementById('start-btn-main');
    if (startBtn) startBtn.addEventListener('click', () => {
        focusWindow('terminal');
    });

    // Expose global functions for HTML debugging
    window.focusWindow = focusWindow;
    window.closeWindow = closeWindow;
}

function updateClock() {
    const clock = document.getElementById('clock');
    if (clock) {
        const now = new Date();
        clock.textContent = now.getHours().toString().padStart(2,'0') + ':' + now.getMinutes().toString().padStart(2,'0');
    }
}

// ============================================================================
// Icon render
// ============================================================================

function renderIcons() {
    const container = document.getElementById('icons-container');
    if (!container) return;
    container.innerHTML = '';
    ICONS.forEach(icon => {
        const div = document.createElement('div');
        div.className = 'desktop-icon';
        div.innerHTML = `<img src="${icon.img}" width="42" height="42"><span>${icon.label}</span>`;
        div.addEventListener('click', (e) => {
            e.stopPropagation();
            focusWindow(icon.id);
        });
        container.appendChild(div);
    });
}

// ============================================================================
// Windows Management
// ============================================================================

function focusWindow(id) {
    const win = STATE.windows.find(w => w.id === id);
    if (win) {
        win.isOpen = true;
        win.z = Math.max(...STATE.windows.map(w => w.z)) + 1;
        renderWindows();
        renderTaskbar();
    }
}

function closeWindow(id) {
    const win = STATE.windows.find(w => w.id === id);
    if (win) {
        win.isOpen = false;
        renderWindows();
        renderTaskbar();
    }
}

function renderTaskbar() {
    const container = document.getElementById('taskbar-apps');
    if (!container) return;
    container.innerHTML = '';
    STATE.windows.filter(w => w.isOpen).forEach(win => {
        const item = document.createElement('div');
        const isActive = win.z === Math.max(...STATE.windows.filter(w => w.isOpen).map(w => w.z));
        item.className = `task-item ${isActive ? 'active' : ''}`;
        item.innerHTML = `<span class="truncate">${win.title}</span>`;
        item.addEventListener('click', () => focusWindow(win.id));
        container.appendChild(item);
    });
}

// ============================================================================
// Windows Render
// ============================================================================

function renderWindows() {
    const container = document.getElementById('windows-container');
    if (!container) return;
    container.innerHTML = '';

    STATE.windows.filter(w => w.isOpen).forEach(win => {
        const el = document.createElement('div');
        el.className = 'pixel-window';
        el.style.left = win.x + 'px';
        el.style.top = win.y + 'px';
        el.style.zIndex = win.z;
        el.style.width = win.width + 'px';
        el.style.height = win.height + 'px';
        const baseWidth = BASE_WIDTHS[win.id] || 400;
        const scale = Math.max(0.7, Math.min(2.5, win.width / baseWidth));
        el.style.setProperty('--win-font-scale', scale);

        // title bar
        const titleBar = document.createElement('div');
        titleBar.className = 'title-bar';
        titleBar.innerHTML = `
            <div class="flex items-center gap-2 pointer-events-none">
                <div class="w-3 h-3 bg-white"></div>
                <span class="font-bold">${win.title}</span>
            </div>
            <div class="win-btn-group flex gap-1">
                <div class="win-btn">_</div>
                <div class="win-btn close-btn">×</div>
            </div>
        `;
        titleBar.onmousedown = (e) => {
            if (e.target.closest('.win-btn')) return;
            focusWindow(win.id);
            let startX = e.clientX - win.x, startY = e.clientY - win.y;
            const move = (me) => { win.x = me.clientX - startX; win.y = me.clientY - startY; el.style.left = win.x + 'px'; el.style.top = win.y + 'px'; };
            const up = () => { document.removeEventListener('mousemove', move); document.removeEventListener('mouseup', up); };
            document.addEventListener('mousemove', move);
            document.addEventListener('mouseup', up);
        };
        titleBar.querySelector('.close-btn').addEventListener('click', e => { e.stopPropagation(); closeWindow(win.id); });

        // body
        const body = document.createElement('div');
        body.className = 'window-body';
        const inner = document.createElement('div');
        inner.className = 'h-full w-full';

        // Render content based on window type
        if (win.type === 'magic') renderMagicCard(inner);
        else if (win.type === 'files') renderFilesContent(inner);
        else if (win.type === 'docs') renderDocsContent(inner);
        else if (win.type === 'chat') renderChatContent(inner);
        else if (win.type === 'terminal') renderTerminalContent(inner);

        // resizer
        const resizer = document.createElement('div');
        resizer.className = 'resize-handle';
        resizer.onmousedown = (e) => {
            e.preventDefault(); e.stopPropagation();
            let startW = win.width, startH = win.height, startX = e.clientX, startY = e.clientY;
            const move = (me) => {
                win.width = Math.max(220, startW + (me.clientX - startX));
                win.height = Math.max(160, startH + (me.clientY - startY));
                el.style.width = win.width + 'px';
                el.style.height = win.height + 'px';
                el.style.setProperty('--win-font-scale', Math.max(0.7, Math.min(2.5, win.width / baseWidth)));
            };
            const up = () => { document.removeEventListener('mousemove', move); document.removeEventListener('mouseup', up); renderWindows(); };
            document.addEventListener('mousemove', move);
            document.addEventListener('mouseup', up);
        };

        body.appendChild(inner);
        el.appendChild(titleBar);
        el.appendChild(body);
        el.appendChild(resizer);
        container.appendChild(el);
    });
}
       
                

// ============================================================================
// render Content Function
// ============================================================================


function renderMagicCard(container) {
    container.innerHTML = `
        <div class="flex flex-col h-full bg-white p-4 border-4 border-purple-100 overflow-auto">
            <div class="flex gap-4 items-start mb-6">
                <div class="w-1/3 flex flex-col items-center gap-3">
                    <div class="p-4 bg-pink-50 border-4 border-dashed border-purple-200">
                        <img src="https://api.iconify.design/pixelarticons:zap.svg?color=%239b7bbe" width="60">
                    </div>
                    <button class="btn-action w-full" id="grass-btn">Magic Grass</button>
                </div>
                <div class="flex-1">
                    <div class="scaled-title mb-1">Webserv OS</div>
                    <div class="scaled-text opacity-70 mb-4">v0.42-preview</div>
                    <div class="scaled-text bg-gray-50 p-2 border-l-4 border-purple-300 italic">
                        "a lightweight http server built with passion and pixels."
                    </div>
                </div>
            </div>
            <div class="border-t-4 border-dashed border-purple-50 pt-4">
                <div class="scaled-text font-bold mb-2">Live Debugger</div>
                <div class="flex gap-2 mb-3">
                    <button class="btn-action demo-load" data-type="txt">Fetch Log</button>
                    <button class="btn-action demo-load" data-type="json">Fetch State</button>
                </div>
                <div class="demo-box scaled-text bg-gray-50 border-4 border-inset p-2 font-mono h-24 overflow-auto">
                    ${STATE.demoContent || 'idle...'}
                </div>
            </div>
        </div>
    `;
    container.querySelector('#grass-btn').onclick = () => focusWindow('jine');
    container.querySelectorAll('.demo-load').forEach(btn => btn.onclick = () => loadDemoData(btn.dataset.type));
}

function renderFilesContent(container) {
    container.innerHTML = `
        <div class="flex flex-col h-full bg-white p-2">
            <div class="mb-2 flex justify-between items-center bg-purple-50 p-2 border-4 border-purple-200">
                <span class="font-bold scaled-text">/uploads</span>
                <input type="file" id="upload-input" style="display:none">
                <button class="btn-action" id="upload-btn">add</button>
            </div>
            ${STATE.uploadMessage ? `
                <div class="mb-2 p-2 border-2 ${STATE.uploadMessageType === 'error' ? 'bg-red-100 border-red-300' : 'bg-green-100 border-green-300'}">
                    <span class="scaled-text ${STATE.uploadMessageType === 'error' ? 'text-red-700' : 'text-green-700'}">${STATE.uploadMessage}</span>
                </div>
            ` : ''}
            <div class="flex-1 overflow-auto border-4 border-purple-100 p-1">
                ${STATE.files.length === 0 ? '<div class="opacity-30 p-10 text-center scaled-text">empty</div>' : ''}
                ${STATE.files.map((f, i) => `
                    <div class="flex justify-between items-center p-2 border-2 border-purple-100 mb-2 bg-white">
                        <div class="flex flex-col">
                            <span class="scaled-text font-bold">${f.name}</span>
                            <span class="text-[10px] opacity-50">${f.size} bytes</span>
                        </div>
                        <button class="px-2 py-1 bg-red-400 border-2 border-black text-white text-[10px]" data-filename="${f.name}" id="del-${i}">kill</button>
                    </div>
                `).join('')}
            </div>
        </div>
    `;
    const input = container.querySelector('#upload-input');
    const uploadBtn = container.querySelector('#upload-btn');
    if (uploadBtn && input) {
        uploadBtn.onclick = () => input.click();
        input.onchange = (e) => {
            const file = e.target.files[0];
            if (file) {
                uploadFileToServer(file);
            }
        };
    }
    // Bind delete button events
    STATE.files.forEach((f, i) => {
        const btn = container.querySelector(`#del-${i}`);
        if (btn) {
            btn.onclick = () => {
                deleteFileFromServer(f.name);
            };
        }
    });
}

// Upload file to server
async function uploadFileToServer(file) {
    // Check file size (10 MB = 10485760 bytes)
    const maxSize = 10 * 1024 * 1024;
    if (file.size > maxSize) {
        const sizeMB = (file.size / (1024 * 1024)).toFixed(2);
        const errorMsg = `error: file too large - ${sizeMB}MB exceeds 10MB limit`;
        console.error(errorMsg);
        STATE.uploadMessage = errorMsg;
        STATE.uploadMessageType = 'error';
        STATE.terminalHistory.push(errorMsg);
        renderWindows();
        // Clear message after 3 seconds
        setTimeout(() => {
            STATE.uploadMessage = null;
            STATE.uploadMessageType = null;
            renderWindows();
        }, 3000);
        return;
    }
    
    const formData = new FormData();
    formData.append('file', file);
    
    try {
        const response = await fetch('/uploads', {
            method: 'POST',
            body: formData
        });
        
        if (response.ok) {
            const successMsg = `success: ${file.name} uploaded (${(file.size / 1024).toFixed(2)}KB)`;
            STATE.uploadMessage = successMsg;
            STATE.uploadMessageType = 'success';
            STATE.terminalHistory.push(successMsg);
            // Refresh file list
            await fetchServerFiles();
            renderWindows();
            // Clear message after 3 seconds
            setTimeout(() => {
                STATE.uploadMessage = null;
                STATE.uploadMessageType = null;
                renderWindows();
            }, 3000);
        } else if (response.status === 413) {
            const errorMsg = `error: payload too large - server rejected`;
            console.error('Upload failed with 413:', response.statusText);
            STATE.uploadMessage = errorMsg;
            STATE.uploadMessageType = 'error';
            STATE.terminalHistory.push(errorMsg);
            renderWindows();
            // Clear message after 3 seconds
            setTimeout(() => {
                STATE.uploadMessage = null;
                STATE.uploadMessageType = null;
                renderWindows();
            }, 3000);
        } else {
            console.error('Upload failed:', response.statusText);
            const errorMsg = `error: upload failed - ${response.statusText}`;
            STATE.uploadMessage = errorMsg;
            STATE.uploadMessageType = 'error';
            STATE.terminalHistory.push(errorMsg);
            renderWindows();
            // Clear message after 3 seconds
            setTimeout(() => {
                STATE.uploadMessage = null;
                STATE.uploadMessageType = null;
                renderWindows();
            }, 3000);
        }
    } catch (error) {
        console.error('Upload error:', error);
        const errorMsg = `error: upload exception - ${error.message}`;
        STATE.uploadMessage = errorMsg;
        STATE.uploadMessageType = 'error';
        STATE.terminalHistory.push(errorMsg);
        renderWindows();
        // Clear message after 3 seconds
        setTimeout(() => {
            STATE.uploadMessage = null;
            STATE.uploadMessageType = null;
            renderWindows();
        }, 3000);
    }
}

// Delete file from server
async function deleteFileFromServer(filename) {
    try {
        const response = await fetch(`/uploads/${filename}`, {
            method: 'DELETE'
        });
        
        if (response.ok) {
            // Refresh file list
            await fetchServerFiles();
            renderWindows();
        } else {
            console.error('Delete failed:', response.statusText);
            STATE.terminalHistory.push(`error: delete failed - ${response.statusText}`);
            renderWindows();
        }
    } catch (error) {
        console.error('Delete error:', error);
        STATE.terminalHistory.push(`error: delete exception - ${error.message}`);
        renderWindows();
    }
}

// Fetch file list from server
async function fetchServerFiles() {
    try {
        const response = await fetch('/uploads');
        if (response.ok) {
            const html = await response.text();
            // Parse file list from HTML (simple parsing)
            let files = parseFileListFromHTML(html);
            // Asynchronously get each file's size
            files = await enrichFilesWithSize(files);
            STATE.files = files;
        } else {
            console.error('Fetch files failed:', response.statusText);
            STATE.files = [];
        }
    } catch (error) {
        console.error('Fetch error:', error);
        STATE.files = [];
    }
}

// Parse file list from autoindex HTML response
function parseFileListFromHTML(html) {
    const files = [];
    // Simple regex to extract href
    const linkRegex = /<a\s+href="([^"]+)">([^<]+)<\/a>/gi;
    let match;
    
    while ((match = linkRegex.exec(html)) !== null) {
        const filename = match[2].trim();
        // Skip directories and special links
        if (filename !== '../' && filename !== '/' && !filename.startsWith('?')) {
            files.push({
                name: filename,
                size: 0
            });
        }
    }
    
    return files;
}

// Asynchronously get file sizes (try multiple methods)
async function enrichFilesWithSize(files) {
    for (const file of files) {
        try {
            // Try to get Content-Length via GET request
            const response = await fetch(`/uploads/${encodeURIComponent(file.name)}`);
            const contentLength = response.headers.get('Content-Length');
            if (contentLength && contentLength !== '0') {
                file.size = parseInt(contentLength);
            } else {
                // If Content-Length is 0 or missing, try to read full response to calculate size
                const blob = await response.blob();
                file.size = blob.size;
            }
        } catch (error) {
        }
    }
    return files;
}

// Fetch documentation metadata (file list)
async function fetchDocsMetadata() {
    try {
        // Method 1: Try to get from /docs autoindex
        const response = await fetch('/docs');
        
        if (response.ok) {
            const html = await response.text();
            
            // Parse md file list from HTML
            const linkRegex = /<a\s+href="([^"]+)">([^<]+)<\/a>/gi;
            const docs = [];
            let match;
            
            while ((match = linkRegex.exec(html)) !== null) {
                const filename = match[2].trim();
                // Only extract .md files, skip ../ etc.
                if (filename.endsWith('.md')) {
                    docs.push(filename);
                }
            }
            
            if (docs.length > 0) {
                STATE.docs = docs;
                return;
            }
        } else {
            console.warn('Fetch docs failed with status:', response.status);
        }
        
        // Method 2: If autoindex fails, try to probe common files
        const commonDocs = ['CGI.md', 'epoll.md', 'functions_expl.md', 'Http.md'];
        const availableDocs = [];
        
        for (const docName of commonDocs) {
            try {
                const resp = await fetch(`/docs/${docName}`, { method: 'HEAD' });
                if (resp.ok) {
                    availableDocs.push(docName);
                }
            } catch (e) {
            }
        }
        
        STATE.docs = availableDocs.length > 0 ? availableDocs : commonDocs;
        
    } catch (error) {
        console.error('Fetch docs error:', error);
        // Fallback to hardcoded list
        STATE.docs = ['CGI.md', 'epoll.md', 'functions_expl.md', 'Http.md'];
    }
}

// Fetch individual document content
async function fetchDocContent(docname) {
    try {
        const response = await fetch(`/docs/${docname}`);
        
        if (response.ok) {
            const content = await response.text();
            STATE.activeDoc = docname;
            STATE.activeDocContent = content;
        } else {
            console.error('Fetch doc content failed:', response.status, response.statusText);
            STATE.activeDocContent = `Error: Failed to load ${docname} (Status: ${response.status})`;
        }
    } catch (error) {
        console.error('Fetch doc content error:', error);
        STATE.activeDocContent = `Error: ${error.message}`;
    }
}

function renderDocsContent(container) {
    if (STATE.activeDoc) {
        container.innerHTML = `
            <div class="h-full flex flex-col bg-white p-3">
                <div class="flex justify-between items-center border-b-4 border-purple-200 pb-2 mb-2">
                    <span class="font-bold scaled-text">${STATE.activeDoc.name}</span>
                    <button class="btn-action" id="back-docs">close</button>
                </div>
                <div class="flex-1 overflow-auto whitespace-pre-wrap scaled-text p-2 bg-gray-50 border-2 border-purple-50">
                    ${STATE.activeDocContent}
                </div>
            </div>
        `;
        container.querySelector('#back-docs').onclick = () => { 
            STATE.activeDoc = null;
            STATE.activeDocContent = '';
            renderWindows(); 
        };
    } else {
        container.innerHTML = `
            <div class="flex flex-col h-full bg-white p-2">
                <div class="font-bold scaled-text mb-2 border-b-4 border-purple-200 pb-1">Documentation</div>
                <div class="flex-1 overflow-auto">
                    ${STATE.docs.length === 0 ? '<div class="opacity-30 p-10 text-center scaled-text">loading...</div>' : ''}
                    ${STATE.docs.map(doc => `
                        <div class="doc-item flex justify-between items-center p-2 border-2 border-purple-100 mb-2 cursor-pointer hover:bg-purple-50" 
                             data-name="${doc}">
                            <span class="scaled-text">${doc}</span>
                            <span class="text-[10px] opacity-40">READ</span>
                        </div>
                    `).join('')}
                </div>
            </div>
        `;
        // Use class selector with dataset to get filename
        container.querySelectorAll('.doc-item').forEach(el => {
            el.onclick = async () => {
                const docName = el.dataset.name;
                await fetchDocContent(docName);
                renderWindows();
            };
        });
    }
}

function renderChatContent(container) {
    container.innerHTML = `
        <div class="flex flex-col h-full bg-white border-4 border-purple-200 p-2 shadow-inner">
            <div class="flex justify-between items-center border-b-4 border-purple-200 pb-2 mb-2">
                <span class="font-bold scaled-text">JINE - Companion</span>
                <button class="btn-action" id="jine-restart" style="font-size: 0.8em; padding: 2px 6px;">restart</button>
            </div>
            <div id="chat-scroller" class="flex-1 overflow-auto flex flex-col gap-2 mb-2 p-1">
                ${STATE.chatHistory.map(m => `
                    <div class="p-2 border-2 border-purple-100 ${m.role === 'assistant' ? 'bg-purple-50 self-start' : 'bg-pink-50 self-end'} max-w-[90%] scaled-text" style="font-size: 0.9em">
                        ${m.text}
                    </div>
                `).join('')}
            </div>
            <div class="flex gap-2 border-t-4 border-purple-100 pt-2">
                <input type="text" id="chat-input" class="flex-1 scaled-text" style="font-size: 0.8em" placeholder="ping companion..." autocomplete="off">
                <button class="btn-action" id="chat-send">send</button>
            </div>
        </div>
    `;
    const input = container.querySelector('#chat-input');
    const sendBtn = container.querySelector('#chat-send');
    const restartBtn = container.querySelector('#jine-restart');
    const scroller = container.querySelector('#chat-scroller');
    if (scroller) scroller.scrollTop = scroller.scrollHeight;

    // Generate random reply
    const generateReply = () => {
        const replies = [
            "system online. how can i help today? (〃^ω^〃)",
            "processing your request... ✧・ﾟ: *✧・ﾟ:*",
            "understood. standing by.",
            "affirmative! ^_^",
            "your wish is my command~",
            "analyzing input... please wait.",
            "ready for next instruction.",
            "all systems nominal. ✨",
            "heartbeat detected. continuing...",
            "jine protocol active. 💜"
        ];
        return replies[Math.floor(Math.random() * replies.length)];
    };

    const sendMessage = async () => {
        const val = input.value.trim();
        if (!val) return;
        STATE.chatHistory.push({ role: 'user', text: val });
        localStorage.setItem('webserv_chat_v3', JSON.stringify(STATE.chatHistory));
        input.value = ''; 
        renderWindows();
        
        // Simulate delayed auto-reply
        setTimeout(() => {
            const reply = generateReply();
            STATE.chatHistory.push({ role: 'assistant', text: reply });
            localStorage.setItem('webserv_chat_v3', JSON.stringify(STATE.chatHistory));
            renderWindows();
        }, 500 + Math.random() * 1000);
    };
    
    const restartChat = () => {
        STATE.chatHistory = [{ role: 'assistant', text: 'system rebooting... ⚡' }];
        setTimeout(() => {
            STATE.chatHistory = [{ role: 'assistant', text: 'system online. how can i help today? (〃^ω^〃)' }];
            localStorage.setItem('webserv_chat_v3', JSON.stringify(STATE.chatHistory));
            renderWindows();
        }, 1000);
        localStorage.setItem('webserv_chat_v3', JSON.stringify(STATE.chatHistory));
        renderWindows();
    };
    
    if (sendBtn) sendBtn.onclick = sendMessage;
    if (restartBtn) restartBtn.onclick = restartChat;
    if (input) input.onkeydown = (e) => { if (e.key === 'Enter') sendMessage(); };
}

function renderTerminalContent(container) {
    container.innerHTML = `
        <div class="terminal-body">
            <div class="terminal-history" id="term-history">
                ${STATE.terminalHistory.join('\n')}
            </div>
            <div class="terminal-input-line">
                <span>$</span>
                <input type="text" class="terminal-input" id="term-input" autofocus spellcheck="false" autocomplete="off">
            </div>
        </div>
    `;
    const input = container.querySelector('#term-input');
    const history = container.querySelector('#term-history');
    if (history) history.scrollTop = history.scrollHeight;
    if (input) input.onkeydown = (e) => {
        if (e.key === 'Enter') {
            processCommand(input.value);
            input.value = '';
            renderWindows();
        }
    };
    container.onclick = () => input.focus();
}

// Complete missing functions
function loadDemoData(type) {
    STATE.demoContent = type === 'json' ? JSON.stringify(STATE.files, null, 2) : "Fetching logs...\n[OK] GET /index.html 200\n[OK] POST /api/chat 201";
    renderWindows();
}

function processCommand(cmd) {
    const c = cmd.toLowerCase().trim();
    STATE.terminalHistory.push(`$ ${cmd}`);
    if (c === 'help') {
        STATE.terminalHistory.push("Available: ls, clear, help, about");
    } else if (c === 'ls') {
        STATE.terminalHistory.push(STATE.files.map(f => f.name).join('  '));
    } else if (c === 'clear') {
        STATE.terminalHistory = [];
    } else {
        STATE.terminalHistory.push(`command not found: ${c}`);
    }
}
