import { NavLink, useNavigate } from 'react-router-dom';
import { useAuth } from '../context/AuthContext';
import {
  LayoutDashboard, Map, Bell, ShieldAlert, Settings, LogOut, Wifi, WifiOff,
  Users, BarChart3
} from 'lucide-react';

const navItems = [
  { to: '/', icon: LayoutDashboard, label: 'Dashboard' },
  { to: '/map', icon: Map, label: 'Live Map' },
  { to: '/alerts', icon: Bell, label: 'Alerts' },
  { to: '/sos', icon: ShieldAlert, label: 'Emergency SOS' },
  { to: '/settings', icon: Settings, label: 'Settings' },
];

const adminItems = [
  { to: '/admin', icon: BarChart3, label: 'Admin Overview' },
  { to: '/admin/riders', icon: Users, label: 'All Riders' },
];

export default function Sidebar({ isConnected }) {
  const { user, logout } = useAuth();
  const navigate = useNavigate();
  const isAdmin = user?.is_staff === true;

  const handleLogout = () => {
    logout();
    navigate('/login');
  };

  return (
    <aside className="sidebar">
      <div className="sidebar-logo">
        <div className="logo-icon">🏍️</div>
        <div>
          <h1>SmartHelmetX</h1>
          <span>IoT Safety System</span>
        </div>
      </div>

      <nav className="sidebar-nav">
        {navItems.map(({ to, icon: Icon, label }) => (
          <NavLink
            key={to}
            to={to}
            end={to === '/'}
            className={({ isActive }) => `nav-link ${isActive ? 'active' : ''}`}
          >
            <Icon size={20} />
            {label}
          </NavLink>
        ))}

        {isAdmin && (
          <>
            <div className="sidebar-divider">
              <span>Admin Panel</span>
            </div>
            {adminItems.map(({ to, icon: Icon, label }) => (
              <NavLink
                key={to}
                to={to}
                end={to === '/admin'}
                className={({ isActive }) => `nav-link admin-link ${isActive ? 'active' : ''}`}
              >
                <Icon size={20} />
                {label}
              </NavLink>
            ))}
          </>
        )}
      </nav>

      <div className="sidebar-footer">
        <div className={`conn-status ${isConnected ? 'online' : 'offline'}`}>
          <div className={`conn-dot ${isConnected ? 'online' : 'offline'}`} />
          {isConnected ? (
            <><Wifi size={14} /> Live Connected</>
          ) : (
            <><WifiOff size={14} /> Disconnected</>
          )}
        </div>
        {user?.rider_id && (
          <div className="sidebar-rider-id">
            <span className="rider-id-badge" style={{ fontSize: 11 }}>{user.rider_id}</span>
          </div>
        )}
        <button className="nav-link" onClick={handleLogout} style={{ marginTop: 4 }}>
          <LogOut size={20} />
          {user?.username || 'Logout'}
        </button>
      </div>
    </aside>
  );
}
