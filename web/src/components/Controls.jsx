const BASE = `
  flex-1 py-2.5 font-display font-semibold text-sm tracking-widest uppercase
  border rounded transition-all duration-150
  disabled:opacity-30 disabled:cursor-not-allowed
`

function LockButton({ onClick, disabled, colorClass, children }) {
  return (
    <button onClick={onClick} disabled={disabled} className={`${BASE} ${colorClass}`}>
      {children}
    </button>
  )
}

export default function Controls({ lockStatus, onArm, onDisarm }) {
  const { state } = lockStatus

  // ARM is only available from DISARMED
  const canArm    = state === 'DISARMED'
  // DISARM is available from any active state
  const canDisarm = state !== 'DISARMED'

  // ARM visual: highlighted green when available, dim active-indicator when already running
  const armColor = canArm
    ? 'border-green text-green hover:bg-green hover:text-bg'
    : 'border-green/20 bg-green/5 text-green/30'

  // DISARM visual: neutral ghost — deliberately not alarming
  const disarmColor = 'enabled:border-border enabled:text-muted enabled:hover:border-text enabled:hover:text-text'

  return (
    <div className="reveal reveal-3 shrink-0 flex gap-2 p-4 border-b border-border">
      <LockButton onClick={onArm}    disabled={!canArm}    colorClass={armColor}>ARM</LockButton>
      <LockButton onClick={onDisarm} disabled={!canDisarm} colorClass={disarmColor}>DISARM</LockButton>
    </div>
  )
}
