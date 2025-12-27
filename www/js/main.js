
// =========================================
// Main JavaScript File
// =========================================

//Check server status on page load
document.addEventListener('DOMContentLoaded', function() {
    checkServerStatus();
    
    // Check status every 30 seconds
    setInterval(checkServerStatus, 30000);
});

// Function to check if server is responding
function checkServerStatus() {
    const statusElement = document.getElementById('server-status');
    if (!statusElement) return;
    
    fetch('/')
        .then(response => {
            if (response.ok) {
                statusElement.textContent = 'Online';
                statusElement.className = 'status-online';
            } else {
                statusElement.textContent = 'Issues Detected';
                statusElement.className = 'status-warning';
            }
        })
        .catch(error => {
            statusElement.textContent = 'Offline';
            statusElement.className = 'status-offline';
        });
}

// Function to load different content types (demo section)
function loadContent(url) {
    const displayElement = document.getElementById('content-display');
    if (!displayElement) return;
    
    displayElement.innerHTML = '<p>Loading...</p>';
    
    fetch(url)
        .then(response => {
            if (!response.ok) {
                throw new Error('HTTP error ' + response.status);
            }
            return response.text();
        })
        .then(data => {
            displayElement.innerHTML = '<pre>' + escapeHtml(data) + '</pre>';
        })
        .catch(error => {
            displayElement.innerHTML = '<p class="error">Error loading content: ' + error.message + '</p>';
        });
}

// Escape HTML to prevent XSS
function escapeHtml(unsafe) {
    return unsafe
        .replace(/&/g, "&amp;")
        .replace(/</g, "&lt;")
        .replace(/>/g, "&gt;")
        .replace(/"/g, "&quot;")
        .replace(/'/g, "&#039;");
}

// Smooth scroll for anchor links
document.querySelectorAll('a[href^="#"]').forEach(anchor => {
    anchor.addEventListener('click', function (e) {
        e.preventDefault();
        const target = document.querySelector(this.getAttribute('href'));
        if (target) {
            target.scrollIntoView({
                behavior: 'smooth',
                block: 'start'
            });
        }
    });
});

// Add animation on scroll
const observerOptions = {
    threshold: 0.1,
    rootMargin: '0px 0px -100px 0px'
};

const observer = new IntersectionObserver(function(entries) {
    entries.forEach(entry => {
        if (entry.isIntersecting) {
            entry.target.style.opacity = '1';
            entry.target.style.transform = 'translateY(0)';
        }
    });
}, observerOptions);

// Observe all feature cards
document.querySelectorAll('.feature-card').forEach(card => {
    card.style.opacity = '0';
    card.style.transform = 'translateY(20px)';
    card.style.transition = 'opacity 0.6s ease, transform 0.6s ease';
    observer.observe(card);
});
