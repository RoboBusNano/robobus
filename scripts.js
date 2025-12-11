// Animación de la imagen central
const image = document.getElementById('mainImage');
let animationPaused = false;

image.addEventListener('mouseover', () => {
    if(!animationPaused) {
        image.style.animationPlayState = 'paused';
    }
});

image.addEventListener('mouseout', () => {
    if(!animationPaused) {
        image.style.animationPlayState = 'running';
    }
});

image.addEventListener('click', () => {
    image.style.transform = 'scale(1.15) rotate(5deg)';
    setTimeout(() => {
        image.style.transform = 'scale(1) rotate(0deg)';
    }, 500);
    
    // Efecto de partículas
    const particles = document.createElement('div');
    particles.style.position = 'absolute';
    particles.style.width = '10px';
    particles.style.height = '10px';
    particles.style.background = 'radial-gradient(circle, var(--secondary-color), transparent)';
    particles.style.borderRadius = '50%';
    particles.style.animation = 'particle 1s ease-out';
    
    const rect = image.getBoundingClientRect();
    particles.style.left = `${rect.left + rect.width/2}px`;
    particles.style.top = `${rect.top + rect.height/2}px`;
    
    document.body.appendChild(particles);
    
    setTimeout(() => {
        particles.remove();
    }, 1000);
    
    animationPaused = !animationPaused;
    image.style.animationPlayState = animationPaused ? 'paused' : 'running';
});

// Funcionalidad del visor de PDF
const modal = document.getElementById('pdfModal');
const pdfViewer = document.getElementById('pdfViewer');
const closeBtn = document.querySelector('.close-btn');

document.querySelectorAll('.view-btn').forEach(btn => {
    btn.addEventListener('click', () => {
        const pdfPath = btn.getAttribute('data-pdf');
        pdfViewer.src = pdfPath;
        modal.style.display = 'block';
    });
});

closeBtn.addEventListener('click', () => {
    modal.style.display = 'none';
    pdfViewer.src = '';
});

window.addEventListener('click', (e) => {
    if (e.target === modal) {
        modal.style.display = 'none';
        pdfViewer.src = '';
    }
});

window.addEventListener('load', () => {
    image.style.opacity = '1';
    image.style.transform = 'translateY(0)';
});