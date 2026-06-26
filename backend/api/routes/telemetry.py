from fastapi import APIRouter
from starlette.exceptions import HTTPException
from backend.api.database import get_connection
from backend.api.schemas import TelemetryResponse


router = APIRouter(prefix="/telemetry", tags=["telemetry"])

@router.get("/", response_model=list[TelemetryResponse])

def get_telemetry():
    try:
        conn = get_connection()
        cursor = conn.cursor()

        cursor.execute(
                        """
                        SELECT id, timestamp, temperature, humidity, machine_status 
                        FROM telemetry 
                        ORDER BY timestamp
                        DESC LIMIT 100
                        """
        )
        
        rows = cursor.fetchall()
        
        return [dict(row) for row in rows]
    
    except Exception as exc:
        raise HTTPException(status_code=500, detail=str("Failed to retrieve telemetry data: " + str(exc))) from exc
    
    finally:     
        if conn is not None:       
            conn.close()



