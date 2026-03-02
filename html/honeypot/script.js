document.addEventListener('DOMContentLoaded', () => {
    const probeBtn = document.getElementById('probeBtn');
    const clearBtn = document.getElementById('clearBtn');
    const statusPanel = document.getElementById('statusPanel');
    const statusText = document.getElementById('statusText');

    const customAlert = document.getElementById('customAlert');
    const closeModalBtn = document.getElementById('closeModalBtn');

    
    if (localStorage.getItem('taransvar_tagged') === 'true') {
        setTaggedUI();
    }

    probeBtn.addEventListener('click', () => {
        
        localStorage.setItem('taransvar_tagged', 'true');

        
        setTaggedUI();

        
        customAlert.classList.add('show');
    });

    closeModalBtn.addEventListener('click', () => {
        customAlert.classList.remove('show');
    });

    clearBtn.addEventListener('click', () => {
        
        localStorage.removeItem('taransvar_tagged');

        
        statusPanel.classList.remove('tagged');
        statusText.textContent = "Network Status: Monitoring";
        probeBtn.disabled = false;
        probeBtn.style.opacity = '1';
    });

    function setTaggedUI() {
        statusPanel.classList.add('tagged');
        statusText.textContent = "Status: THREAT DETECTED & TAGGED";
        probeBtn.disabled = true;
        probeBtn.style.opacity = '0.5';
    }
});
