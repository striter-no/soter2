from enum import Enum

class APIStatus(Enum):
    UNDEFINED = -1

    OK = 0
    FAIL = 1

class APISystemStatus(Enum):
    UNDEFINED = -1

    ACTIVE = 0
    NON_ACTIVE = 1

def parse_api_status(status: str) -> APIStatus:
    match status:
        case "ok":
            return APIStatus.OK
        case "fail":
            return APIStatus.FAIL
    return APIStatus.UNDEFINED

def parse_sys_api_status(status: str) -> APISystemStatus:
    match status:
        case "active":
            return APISystemStatus.ACTIVE
        case "nonactive":
            return APISystemStatus.NON_ACTIVE
    return APISystemStatus.UNDEFINED
