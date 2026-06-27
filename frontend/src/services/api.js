import axios from 'axios';

const API_BASE = import.meta.env.VITE_API_BASE_URL || 'http://127.0.0.1:8000/api';

const api = axios.create({
  baseURL: API_BASE,
  headers: { 'Content-Type': 'application/json' },
});

// Attach JWT token to every request
api.interceptors.request.use((config) => {
  const tokens = JSON.parse(localStorage.getItem('tokens') || '{}');
  if (tokens.access) {
    config.headers.Authorization = `Bearer ${tokens.access}`;
  }
  return config;
});

// Auto-refresh on 401
api.interceptors.response.use(
  (res) => res,
  async (error) => {
    const original = error.config;
    if (error.response?.status === 401 && !original._retry) {
      original._retry = true;
      const tokens = JSON.parse(localStorage.getItem('tokens') || '{}');
      if (tokens.refresh) {
        try {
          const res = await axios.post(`${API_BASE}/auth/refresh/`, { refresh: tokens.refresh });
          const newTokens = { access: res.data.access, refresh: res.data.refresh || tokens.refresh };
          localStorage.setItem('tokens', JSON.stringify(newTokens));
          original.headers.Authorization = `Bearer ${newTokens.access}`;
          return api(original);
        } catch {
          localStorage.removeItem('tokens');
          localStorage.removeItem('user');
          window.location.href = '/login';
        }
      }
    }
    return Promise.reject(error);
  }
);

export const authAPI = {
  login: (data) => api.post('/auth/login/', data),
  register: (data) => api.post('/auth/register/', data),
  profile: () => api.get('/auth/profile/'),
  updateProfile: (data) => api.put('/auth/profile/', data),
};

export const dashboardAPI = {
  get: () => api.get('/dashboard/'),
  sensorHistory: (hours = 1) => api.get(`/sensor-history/?hours=${hours}`),
  gpsHistory: (hours = 1) => api.get(`/gps-history/?hours=${hours}`),
};

export const alertsAPI = {
  list: (params = {}) => api.get('/alerts/', { params }),
  resolve: (id) => api.post(`/alerts/${id}/resolve/`),
  cancel: (id) => api.post(`/alerts/${id}/cancel/`),
};

export const contactsAPI = {
  list: () => api.get('/contacts/'),
  create: (data) => api.post('/contacts/', data),
  update: (id, data) => api.put(`/contacts/${id}/`, data),
  delete: (id) => api.delete(`/contacts/${id}/`),
};

export const devicesAPI = {
  list: () => api.get('/devices/'),
  update: (id, data) => api.patch(`/devices/${id}/`, data),
};

export const sosAPI = {
  trigger: () => api.post('/sos/'),
};

export const adminAPI = {
  stats: () => api.get('/admin/stats/'),
  riders: (search = '') => api.get(`/admin/riders/${search ? `?search=${search}` : ''}`),
  riderDetail: (riderId) => api.get(`/admin/riders/${riderId}/`),
  updateRider: (riderId, data) => api.patch(`/admin/riders/${riderId}/`, data),
};

export default api;
