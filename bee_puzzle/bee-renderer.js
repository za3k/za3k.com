// Canvas setup
const canvas = document.getElementById('hive');
const ctx = canvas.getContext('2d');

const canvasSize = 650;
const hiveOffset = 50;
const hiveSize = 550;
const hiveCenterX = hiveOffset + hiveSize / 2;
const hiveCenterY = hiveOffset + hiveSize / 2;
const beeSize = 18;
const waggleLength = 200;
const loopWidth = waggleLength / 5;
const loopHeight = waggleLength / 2;

// Key points
const offscreenLeft = { x: -50, y: -50 };
const offscreenRight = { x: canvasSize + 50, y: -50 };
const leftDance = { x: hiveOffset + hiveSize * 0.3, y: hiveCenterY };
const rightDance = { x: hiveOffset + hiveSize * 0.7, y: hiveCenterY };

// Timing
const FLY_DURATION = 300;
const WAGGLE_DURATION = 225;
const ROTATE_DURATION = 225;

let globalTime = 0;

class Bee {
    constructor() {
        this.x = -100;
        this.y = -100;
        this.angle = 0;
        this.visible = false;
        this.isWaggling = false;
        this.actions = [];
        this.actionIndex = 0;
        this.actionStartTime = 0;
        this.startDelay = 0;
    }
    
    teleportTo(point) {
        this.actions.push({ type: 'teleport', target: point });
        return this;
    }
    
    flyTo(point) {
        this.actions.push({ type: 'fly', target: point, duration: FLY_DURATION });
        return this;
    }
    
    waggleTo(point) {
        this.actions.push({ type: 'waggle', target: point, duration: WAGGLE_DURATION });
        return this;
    }
    
    rotateTo(point, rightSide) {
        this.actions.push({ type: 'rotate', target: point, rightSide: rightSide, duration: ROTATE_DURATION });
        return this;
    }
    
    setStartDelay(delay) {
        this.startDelay = delay;
        return this;
    }
    
    update(time) {
        const localTime = time - this.startDelay;
        if (localTime < 0) {
            this.visible = false;
            return;
        }
        
        this.visible = true;
        this.isWaggling = false;
        
        let elapsed = 0;
        for (let i = 0; i < this.actions.length; i++) {
            const action = this.actions[i];
            const duration = action.duration || 0;
            
            if (duration === 0) {
                this.executeAction(action, 0, i);
                continue;
            }
            
            if (localTime < elapsed + duration) {
                this.executeAction(action, localTime - elapsed, i);
                return;
            }
            elapsed += duration;
        }
        
        this.visible = false;
    }
    
    executeAction(action, actionTime, actionIndex) {
        const progress = action.duration ? Math.min(1, actionTime / action.duration) : 1;
        const eased = progress * progress * (3 - 2 * progress);
        
        let prevPos = this.getPreviousPosition(actionIndex);
        
        switch (action.type) {
            case 'teleport':
                this.x = action.target.x;
                this.y = action.target.y;
                this.angle = Math.atan2(action.target.y - prevPos.y, action.target.x - prevPos.x);
                break;
                
            case 'fly':
                this.x = prevPos.x + (action.target.x - prevPos.x) * eased;
                this.y = prevPos.y + (action.target.y - prevPos.y) * eased;
                this.angle = Math.atan2(action.target.y - prevPos.y, action.target.x - prevPos.x);
                break;
                
            case 'waggle':
                this.x = prevPos.x + (action.target.x - prevPos.x) * progress;
                this.y = prevPos.y + (action.target.y - prevPos.y) * progress;
                this.angle = Math.atan2(action.target.y - prevPos.y, action.target.x - prevPos.x);
                this.isWaggling = true;
                break;
                
            case 'rotate':
                const midX = (prevPos.x + action.target.x) / 2;
                const midY = (prevPos.y + action.target.y) / 2;
                const dx = action.target.x - prevPos.x;
                const dy = action.target.y - prevPos.y;
                const dist = Math.sqrt(dx * dx + dy * dy);
                
                const perpX = -dy / dist * loopWidth;
                const perpY = dx / dist * loopWidth;
                const sign = action.rightSide ? 1 : -1;
                
                const arcAngle = progress * Math.PI;
                const arcProgress = (1 - Math.cos(arcAngle)) / 2;
                const perpAmount = Math.sin(arcAngle);
                
                this.x = prevPos.x + dx * arcProgress + perpX * perpAmount * sign;
                this.y = prevPos.y + dy * arcProgress + perpY * perpAmount * sign;
                
                const velX = dx / 2 * Math.sin(arcAngle) + perpX * Math.cos(arcAngle) * sign;
                const velY = dy / 2 * Math.sin(arcAngle) + perpY * Math.cos(arcAngle) * sign;
                this.angle = Math.atan2(velY, velX);
                break;
        }
    }
    
    getPreviousPosition(actionIndex) {
        if (actionIndex === 0) {
            return { x: this.actions[0].target?.x || 0, y: this.actions[0].target?.y || 0 };
        }
        
        let pos = { x: 0, y: 0 };
        for (let i = 0; i < actionIndex; i++) {
            const action = this.actions[i];
            if (action.target) {
                pos = { x: action.target.x, y: action.target.y };
            }
        }
        return pos;
    }
    
    draw(time) {
        if (!this.visible) return;
        
        ctx.save();
        ctx.translate(this.x, this.y);
        ctx.rotate(this.angle);
        
        const size = beeSize;
        
        if (this.isWaggling) {
            const waggleRotation = Math.sin(time * 0.8) * 0.4;
            ctx.rotate(waggleRotation);
            const waggleSide = Math.sin(time * 0.8) * 6;
            ctx.translate(0, waggleSide);
        }
        
        const wingSpeed = this.isWaggling ? 1.2 : 0.4;
        ctx.fillStyle = 'rgba(200, 220, 255, 0.7)';
        ctx.beginPath();
        ctx.ellipse(-size * 0.3, -size * 0.8, size * 0.5, size * 0.3, -0.3, 0, Math.PI * 2);
        ctx.fill();
        ctx.beginPath();
        ctx.ellipse(-size * 0.3, size * 0.8, size * 0.5, size * 0.3, 0.3, 0, Math.PI * 2);
        ctx.fill();
        
        const wingFlap = Math.sin(time * wingSpeed) * (this.isWaggling ? 0.4 : 0.2);
        ctx.fillStyle = 'rgba(180, 200, 255, 0.6)';
        ctx.beginPath();
        ctx.ellipse(-size * 0.2, -size * 0.6 + wingFlap * 15, size * 0.4, size * 0.25, -0.3, 0, Math.PI * 2);
        ctx.fill();
        ctx.beginPath();
        ctx.ellipse(-size * 0.2, size * 0.6 - wingFlap * 15, size * 0.4, size * 0.25, 0.3, 0, Math.PI * 2);
        ctx.fill();
        
        ctx.fillStyle = '#1a1a1a';
        ctx.beginPath();
        ctx.ellipse(size * 0.9, 0, size * 0.4, size * 0.35, 0, 0, Math.PI * 2);
        ctx.fill();
        
        ctx.save();
        if (this.isWaggling) {
            const abdomenWaggle = Math.sin(time * 0.85) * 0.3;
            ctx.translate(-size * 0.3, 0);
            ctx.rotate(abdomenWaggle);
            ctx.translate(size * 0.3, 0);
        }
        
        const gradient = ctx.createLinearGradient(-size, 0, size, 0);
        gradient.addColorStop(0, '#FFD700');
        gradient.addColorStop(0.2, '#1a1a1a');
        gradient.addColorStop(0.35, '#FFD700');
        gradient.addColorStop(0.5, '#1a1a1a');
        gradient.addColorStop(0.65, '#FFD700');
        gradient.addColorStop(0.8, '#1a1a1a');
        gradient.addColorStop(1, '#FFD700');
        
        ctx.fillStyle = gradient;
        ctx.beginPath();
        ctx.ellipse(0, 0, size, size * 0.6, 0, 0, Math.PI * 2);
        ctx.fill();
        
        ctx.fillStyle = '#1a1a1a';
        ctx.beginPath();
        ctx.moveTo(-size, 0);
        ctx.lineTo(-size * 1.3, -size * 0.1);
        ctx.lineTo(-size * 1.3, size * 0.1);
        ctx.closePath();
        ctx.fill();
        
        ctx.restore();
        
        ctx.fillStyle = '#333';
        ctx.beginPath();
        ctx.arc(size * 1.05, -size * 0.15, size * 0.12, 0, Math.PI * 2);
        ctx.arc(size * 1.05, size * 0.15, size * 0.12, 0, Math.PI * 2);
        ctx.fill();
        
        ctx.fillStyle = 'rgba(255,255,255,0.5)';
        ctx.beginPath();
        ctx.arc(size * 1.08, -size * 0.18, size * 0.05, 0, Math.PI * 2);
        ctx.arc(size * 1.08, size * 0.12, size * 0.05, 0, Math.PI * 2);
        ctx.fill();
        
        ctx.strokeStyle = '#1a1a1a';
        ctx.lineWidth = 1.5;
        ctx.beginPath();
        ctx.moveTo(size * 1.1, -size * 0.25);
        ctx.quadraticCurveTo(size * 1.4, -size * 0.5, size * 1.3, -size * 0.7);
        ctx.stroke();
        ctx.beginPath();
        ctx.moveTo(size * 1.1, size * 0.25);
        ctx.quadraticCurveTo(size * 1.4, size * 0.5, size * 1.3, size * 0.7);
        ctx.stroke();
        
        ctx.restore();
    }
}

function getWagglePoints(angle, danceCenter) {
    const angleRad = ((angle - 1) / 5) * Math.PI * 2 - Math.PI / 2;
    const dx = Math.cos(angleRad) * waggleLength;
    const dy = Math.sin(angleRad) * waggleLength;
    return {
        start: { x: danceCenter.x - dx/2, y: danceCenter.y - dy/2 },
        end: { x: danceCenter.x + dx/2, y: danceCenter.y + dy/2 }
    };
}

function drawHoneycombPattern() {
    ctx.save();
    ctx.beginPath();
    ctx.arc(hiveCenterX, hiveCenterY, hiveSize / 2 - 2, 0, Math.PI * 2);
    ctx.clip();
    
    ctx.globalAlpha = 0.4;
    ctx.strokeStyle = '#3d2a06';
    ctx.lineWidth = 1.5;
    
    const hexSize = 25;
    for (let row = -10; row < 25; row++) {
        for (let col = -10; col < 25; col++) {
            const x = hiveOffset + col * hexSize * 1.5;
            const y = hiveOffset + row * hexSize * Math.sqrt(3) + (col % 2) * hexSize * Math.sqrt(3) / 2;
            
            ctx.beginPath();
            for (let i = 0; i < 6; i++) {
                const a = (Math.PI / 3) * i;
                const px = x + Math.cos(a) * hexSize;
                const py = y + Math.sin(a) * hexSize;
                if (i === 0) ctx.moveTo(px, py);
                else ctx.lineTo(px, py);
            }
            ctx.closePath();
            ctx.stroke();
        }
    }
    ctx.restore();
}

function runAnimation(bees, totalDuration) {
    function animate() {
        const frameTime = globalTime % totalDuration;
        
        ctx.clearRect(0, 0, canvasSize, canvasSize);
        drawHoneycombPattern();
        
        for (const bee of bees) {
            bee.update(frameTime);
            bee.draw(globalTime);
        }
        
        globalTime++;
        requestAnimationFrame(animate);
    }
    animate();
}
